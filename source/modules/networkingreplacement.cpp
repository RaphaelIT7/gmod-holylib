#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "recipientfilter.h"
#include "packed_entity.h"
#include "dt.h"
#include "framesnapshot.h"
#include "sourcesdk/hltvserver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/*
	This is highly experimental.
	Since I am waiting most of the time for feedback on the networking module
	I decided to implement this partially just for future testing.
	Why focus on this? Because it could be huge for networking.

	Idea:
	Originally had this for RMod though doing this in GMod seems to be way more useful since it'll have a way greater benefit.

	The idea is for each SendTable we setup a new memory block in which we store the raw variables for a snapshot.
	This saves us from storing compressed data - though why wouldn't we want to store compressed data?
	Well... The issue is that when for clients we want to compare deltas we then would need to read the variables from the compressed data
	which adds way more instructions since we need to read bits - so to have it easier for the CPU we store the raw variables to compare with
	This should hopefully be way better for the CPU at the trade of higher memory usage.
	We still can kinda compress it though I wanna limit it at best to align things to 1 byte / no bits (except for bools)

	Importantly - the data we send to the client should remain the same, we change whats going on behind the scenes but not the result

	Knowledge:
	SendTable - This basically is a class containing all networked variables of an networked entity class.

	Currently this module depends on the networking & sourcetv module to work.
*/

class CNetworkingReplacementModule : public IModule
{
public:
	virtual void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax) OVERRIDE;
	virtual void InitDetour(bool bPreServer) OVERRIDE;
	virtual const char* Name() { return "networkingreplacement"; };
	virtual int Compatibility() { return LINUX32; };
	virtual bool IsEnabledByDefault() { return false; };
};

static CNetworkingReplacementModule g_pNetworkingReplacementModule;
IModule* pNetworkingReplacementModule = &g_pNetworkingReplacementModule;

/*
	SDK things to make the linker happy
*/

SendProp::SendProp()
{
	m_pMatchingRecvProp = nullptr;
	m_Type = DPT_Int;
	m_nBits = 0;

	m_fLowValue = 0.0f;
	m_fHighValue = 0.0f;

	m_pArrayProp = nullptr;
	m_ArrayLengthProxy = nullptr;

	m_nElements = 1;
	m_ElementStride = -1;

	m_pExcludeDTName = nullptr;
	m_pParentArrayPropName = nullptr;

	m_pVarName = nullptr;
	m_fHighLowMul = 1.f;

	m_Flags = 0;

	m_ProxyFn = nullptr;
	m_DataTableProxyFn = nullptr;

	m_pDataTable = nullptr;

	m_Offset = 0;

	m_pExtraData = nullptr;
}

SendProp::~SendProp() = default;

static bool PropOffsetLT( const unsigned short &a, const unsigned short &b )
{
	return a < b;
}

CSendTablePrecalc::CSendTablePrecalc() : 
	m_PropOffsetToIndexMap( 0, 0, PropOffsetLT )
{
	m_pDTITable = nullptr;
	m_pSendTable = 0;
	m_nDataTableProxies = 0;
}


CSendTablePrecalc::~CSendTablePrecalc()
{
	if ( m_pSendTable )
		m_pSendTable->m_pPrecalc = 0;
}

CSendNode::CSendNode()
{
	m_iDatatableProp = -1;
	m_pTable = nullptr;
	
	m_iFirstRecursiveProp = m_nRecursiveProps = 0;

	m_DataTableProxyIndex = DATATABLE_PROXY_INDEX_INVALID; // set it to a questionable value.
	m_RecursiveProxyIndex = DATATABLE_PROXY_INDEX_INVALID;
}

CSendNode::~CSendNode()
{
	intp c = GetNumChildren();
	for ( intp i = c - 1 ; i >= 0 ; i-- )
	{
		delete GetChild( i );
	}
	m_Children.Purge();
}

/*
	replacement classes
*/

// Copy class of SendProp but contains extra data for us / specific for our SendTable
class HolyLibSendPropPrecalc : public SendProp
{
public:
	static constexpr int BOOL_SIZE = -1;
	static constexpr int DPT_BOOL = DPT_NUMSendPropTypes;
	static constexpr int DPT_INT24 = DPT_BOOL+1; // ungly, though useful to avoid branching - unused ToDo: Implement it into PrecalcSendProps, too lazy rn
	static constexpr int DPT_INT16 = DPT_INT24+1;
	static constexpr int DPT_INT8 = DPT_INT16+1;
	static constexpr int DPT_REALNUMSendPropTypes = DPT_INT8+1;

	// int m_nNewTypeOffset = 0; // We inherit m_Offset already, this is to map the old offset to our new one relative to the type chunk!
	int m_nNewTotalOffset = 0; // We inherit m_Offset already, this is to map the old offset to our new total offset in the packed entity!
	int m_nNewSize = 0; // Size in our new struct - in bytes! (if -1 then it's a bool!)
	int m_nBitOffset = 0; // For bool types since we pack them into a byte
	unsigned short m_nPropID = 0;
	unsigned char m_nArrayElementSize = 0;
	unsigned char m_nHolyLibType = 0; // our type since we got things like DPT_BOOL though are incompatible when using pPropTypeFns
};

struct int24
{
	int24(int value)
	{
		val[0] = value & 0xFF;
		val[1] = (value >> 8) & 0xFF;
		val[2] = (value >> 16) & 0xFF;
	}

	int ToInt() const
	{
		int value = (val[0]) | (val[1] << 8 ) | (val[2] << 16);

		if (value & 0x00800000)
			value |= 0xFF000000;

		return value;
	}

	char val[3];
};

struct VectorXY
{
	float val[2];
};

// ----------------------------------------------------------------------------- //
// CSendTablePrecalc
// ----------------------------------------------------------------------------- //
class HolyLibCSendTablePrecalc : public CSendTablePrecalc
{
public:
	HolyLibCSendTablePrecalc() {};

	void PrecalcSendProps();

public:
	// Contains all data needed to later create snapshots with
	class SendPropStruct
	{
	public:
		union
		{
			struct
			{
				int m_nBits; // Temporary only
				int m_nBytes; // Final size of our block.
			};
			HolyLibSendPropPrecalc* m_nGModDataTableProp; // Saves us some bytes - used by DPT_GMODTable
		};
		CUtlVector<HolyLibSendPropPrecalc*> m_pProps; // All props of this type
	} m_SendPropStruct[HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes]; // +1 for DPT_BOOL
	int m_nSendPropDataSize = 0;
};

void HolyLibCSendTablePrecalc::PrecalcSendProps()
{
	/*
		Generate SendPropPrecalc data
		This splits all SendProps into groups by their types
		Aligns them to 1 byte / 8 bits and sets new offsets
	*/

	struct SendPropEntry
	{
		SendPropEntry(const SendProp* pProp, int iProp)
		{
			this->pProp = pProp;
			this->iProp = iProp;
		};

		const SendProp* pProp = nullptr;
		int iProp = 0;
	};

	CUtlVector<SendPropEntry> pProps[HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes];
	for (int i = 0; i < GetNumProps(); ++i)
	{
		const SendProp* pProp = GetProp(i);
		SendPropType type = pProp->GetType();
	
		if (type >= 0 && type < DPT_NUMSendPropTypes)
		{
			int nBytes = ((pProp->m_nBits + 7) & ~7) / 8;
			if (type == SendPropType::DPT_Int && pProp->m_nBits == 1)
			{
				pProps[HolyLibSendPropPrecalc::DPT_BOOL].AddToTail({pProp, i});
			} else if (type == SendPropType::DPT_Int && nBytes == 1)
			{
				pProps[HolyLibSendPropPrecalc::DPT_INT8].AddToTail({pProp, i});
			} else if (type == SendPropType::DPT_Int && nBytes == 2)
			{
				pProps[HolyLibSendPropPrecalc::DPT_INT16].AddToTail({pProp, i});
			} else if (type == SendPropType::DPT_Int && nBytes == 3)
			{
				pProps[HolyLibSendPropPrecalc::DPT_INT24].AddToTail({pProp, i});
			} else {
				pProps[type].AddToTail({pProp, i});
			}
		} else {
			Warning(PROJECT_NAME " - HolyLibCSendTablePrecalc::PrecalcSendProps %s has invalid type %d\n", pProp->GetName(), type);
		}
	}

	for (int nDPTType = 0; nDPTType < HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes; ++nDPTType)
	{
		SendPropStruct& pStruct = m_SendPropStruct[nDPTType];
		pStruct.m_nBits = 0;
		pStruct.m_nBytes = 0;

		FOR_EACH_VEC( pProps[nDPTType], i )
		{
			SendPropEntry& pEntry = pProps[nDPTType][i];
			const SendProp* pProp = pEntry.pProp;
			int nBits = pProp->m_nBits;
			switch(nDPTType)
			{
			case SendPropType::DPT_Int:
				nBits = (sizeof(int) * 8);
				break;
			case SendPropType::DPT_Float:
				nBits = sizeof(float) * 8;
				break;
			case SendPropType::DPT_Vector:
				nBits = sizeof(Vector) * 8;
				break;
			case SendPropType::DPT_VectorXY:
				nBits = sizeof(VectorXY) * 8;
				break;
			case SendPropType::DPT_String: // SHIT! SendPropString needs to have this line added IN GMOD, else we can't know the size! ret.m_nBits = bufferLen * 8;
				// IDEA:
				// We instead use a void* pointer.
				// If it's NULL then the string is empty - skip
				// Else it points to the data string which can be used.
				// In the PackedEntity this data string will be put onto the end of our block.
				// Why not direclty keep it? We expect our build data to have constant offsets which wouldn't be possible with strings varying in length.
				nBits = sizeof(void*) * 8; 
				break;
			case SendPropType::DPT_Array:
				nBits = 0; // How will I handle arrays? I don't know yet...
				break;
			case SendPropType::DPT_GMODTable:
				break;
			case HolyLibSendPropPrecalc::DPT_BOOL:
				break;
			case HolyLibSendPropPrecalc::DPT_INT24:
				nBits = sizeof(int24);
				break;
			case HolyLibSendPropPrecalc::DPT_INT16:
				nBits = sizeof(short);
				break;
			case HolyLibSendPropPrecalc::DPT_INT8:
				nBits = sizeof(char);
				break;
			default:
				Warning(PROJECT_NAME " - HolyLibCSendTablePrecalc::PrecalcSendProps: No idea for size of %s!\n", pProp->GetName());
				break;
			}

			HolyLibSendPropPrecalc* pPrecalc = new HolyLibSendPropPrecalc;
			memcpy(pPrecalc, pProp, sizeof(SendProp));
			pPrecalc->m_nPropID = pEntry.iProp;
			pPrecalc->m_nHolyLibType = nDPTType;

			m_Props[pEntry.iProp] = pPrecalc; // We replace all our props too

			if (nDPTType == HolyLibSendPropPrecalc::DPT_BOOL) // Bool, yay, we store them in bits :3
			{
				if (pStruct.m_nBytes == 0) // Initial size! Needed for later allocation as else we'd have to check the bits
					++pStruct.m_nBytes;

				pPrecalc->m_nNewTotalOffset = m_nSendPropDataSize + pStruct.m_nBytes-1; // -1 since m_nBytes will always be +1 for the current byte
				pPrecalc->m_nNewSize = HolyLibSendPropPrecalc::BOOL_SIZE;
				pPrecalc->m_nBitOffset = pStruct.m_nBits++;
				if (pStruct.m_nBits >= 8) // not >= since else we'd skip the 8th bit
				{
					++pStruct.m_nBytes;
					pStruct.m_nBits = 0;
				}

				pStruct.m_pProps.AddToTail(pPrecalc);
			} else if (nDPTType == SendPropType::DPT_GMODTable) {
				pPrecalc->m_nNewTotalOffset = m_nSendPropDataSize + pStruct.m_nBytes;
				pPrecalc->m_nNewSize = sizeof(unsigned short);

				pStruct.m_nBytes += pPrecalc->m_nNewSize;
				pStruct.m_nGModDataTableProp = pPrecalc;
			} else if (nDPTType == SendPropType::DPT_Array) {
				SendProp* pArrayProp = pProp->GetArrayProp();

				pPrecalc->m_Offset = pArrayProp->GetOffset(); // Inherit offset
				pPrecalc->m_nNewTotalOffset = m_nSendPropDataSize + pStruct.m_nBytes;

				pPrecalc->m_nArrayElementSize = ((pArrayProp->m_nBits + 7) & ~7) / 8;
				int nBytes = pPrecalc->GetNumElements() * pPrecalc->m_nArrayElementSize;
				pPrecalc->m_nNewSize = nBytes;


				HolyLibSendPropPrecalc* pPrecalcArrayProp = new HolyLibSendPropPrecalc;
				memcpy(pPrecalcArrayProp, pArrayProp, sizeof(SendProp));

				pPrecalc->m_pArrayProp = pPrecalcArrayProp;
				pPrecalcArrayProp->m_nNewSize = pPrecalc->m_nArrayElementSize;

				if (pPrecalcArrayProp->m_Type == SendPropType::DPT_Int && pArrayProp->m_nBits == 1)
				{
					pPrecalcArrayProp->m_nHolyLibType = HolyLibSendPropPrecalc::DPT_BOOL;
				} else if (pPrecalcArrayProp->m_Type == SendPropType::DPT_Int && pPrecalc->m_nArrayElementSize == 1)
				{
					pPrecalcArrayProp->m_nHolyLibType = HolyLibSendPropPrecalc::DPT_INT8;
				} else if (pPrecalcArrayProp->m_Type == SendPropType::DPT_Int && pPrecalc->m_nArrayElementSize == 2)
				{
					pPrecalcArrayProp->m_nHolyLibType = HolyLibSendPropPrecalc::DPT_INT16;
				} else if (pPrecalcArrayProp->m_Type == SendPropType::DPT_Int && pPrecalc->m_nArrayElementSize == 3)
				{
					pPrecalcArrayProp->m_nHolyLibType = HolyLibSendPropPrecalc::DPT_INT24;
				}

				if (pArrayProp->m_nBits == 1 && pArrayProp->m_Type == DPT_Int)
					Error("Array %s is bool?\n", pArrayProp->GetName()); // Currently this shouldn't be the case - if it becomes the case I'll need to rework some shit

				pStruct.m_nBytes += nBytes;
				pStruct.m_pProps.AddToTail(pPrecalc);
			} else {
				pPrecalc->m_nNewTotalOffset = m_nSendPropDataSize + pStruct.m_nBytes;

				int nBytes = ((nBits + 7) & ~7) / 8; // Align to 8 bits & make into bytes
				pStruct.m_nBytes += nBytes;
				pPrecalc->m_nNewSize = nBytes;

				pStruct.m_pProps.AddToTail(pPrecalc);
			}

			// Msg("%i: %s (%i) - %s\n", i, pProp->GetName(), nBits, pProp->GetArrayProp() ? pProp->GetArrayProp()->GetName() : "");
		}

		m_nSendPropDataSize += pStruct.m_nBytes; // Collect final sendprop size :3
	}
}

/*
	Functions for packing
*/

struct PackState
{
	PackState(void* pPackedData, void* pEntityBase, HolyLibCSendTablePrecalc* pPrecalc)
	{
		m_pPackedData = (unsigned char*)pPackedData;
		m_pEntityBase = (unsigned char*)pEntityBase;
		m_pPrecalc = pPrecalc;
	}

	int BuildPack();
	inline void AddString(const char* pString, int nOffset)
	{
		StringEntry& pEntry = pStringEnties.emplace_back();
		pEntry.nDataOffset = nOffset;
		pEntry.pString = pString;
	}

	unsigned char* m_pPackedData = nullptr;
	unsigned char* m_pEntityBase = nullptr;
	HolyLibCSendTablePrecalc* m_pPrecalc = nullptr;

	struct StringEntry
	{
		const char* pString;
		int nDataOffset; // To update the data pack entry
	};
	std::vector<StringEntry> pStringEnties;
};

static int SendProp_FillSnapshot( HolyLibCSendTablePrecalc *pTable, void* pOutData, void* pEntity )
{
	PackState pState( pOutData, pEntity, pTable );
	return pState.BuildPack();
}

typedef struct
{
	bool			(*IsEncodedZero) ( const HolyLibSendPropPrecalc* pProp, const void* pPackedData );
	// I HATE that the pEntityData argument is needed for the gmod datatable.
	void			(*FillDVariant) ( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar );

	// We need nCurrentOffset since if an string array exists - we need to know the offset in String_PackEntry!
	void			(*PackEntry) ( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState );
} HolyLibPropTypeFns;

extern HolyLibPropTypeFns pHolyLibPropTypeFns[HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes]; // For recursive fun

static bool HolyLib_Int_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	return (*(int*)pPackedData) == 0;
}

static void HolyLib_Int_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	pVar.m_Int = *(int*)pPackedData;
	Msg("Read value %i from %s at %p\n", pVar.m_Int, pProp->GetName(), pPackedData);
}

static void HolyLib_Int_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	*(int*)pPackedData = pVar.m_Int;
	Msg("Packed value %i into %s at %p\n", pVar.m_Int, pProp->GetName(), pPackedData);
}

static bool HolyLib_Float_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	return (*(float*)pPackedData) == 0;
}

static void HolyLib_Float_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	pVar.m_Float = *(float*)pPackedData;
	Msg("Read value %f from %s at %p\n", pVar.m_Float, pProp->GetName(), pPackedData);
}

static void HolyLib_Float_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	*(float*)pPackedData = pVar.m_Float;
	Msg("Packed value %f into %s at %p\n", pVar.m_Float, pProp->GetName(), pPackedData);
}

static bool HolyLib_Vector_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	Vector* pVec = (Vector*)pPackedData;
	return pVec->x == 0 && pVec->y == 0 && pVec->z == 0;
}

static void HolyLib_Vector_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	Vector* pVec = (Vector*)pPackedData;
	pVar.m_Vector[0] = pVec->x;
	pVar.m_Vector[1] = pVec->y;
	pVar.m_Vector[2] = pVec->z;
}

static void HolyLib_Vector_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	*(Vector*)pPackedData = *(Vector*)&pVar.m_Vector;
}

static bool HolyLib_VectorXY_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	VectorXY* pVec = (VectorXY*)pPackedData;
	return pVec->val[0] == 0 && pVec->val[1] == 0;
}

static void HolyLib_VectorXY_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	VectorXY* pVec = (VectorXY*)pPackedData;
	pVar.m_Vector[0] = pVec->val[0];
	pVar.m_Vector[1] = pVec->val[1];
}

static void HolyLib_VectorXY_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	*(VectorXY*)pPackedData = *(VectorXY*)&pVar;
}

static bool HolyLib_String_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	// SPECIAL: Since we store a void* pointer IF the string has data we can check if it's null
	return *(void**)pPackedData == nullptr;
}

static void HolyLib_String_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	const char* pStringPointer = *(const char**)pPackedData;
	if (pStringPointer) // Branching.... ugh Good thing not that many strings exist
	{
		pVar.m_pString = pStringPointer;
	} else {
		pVar.m_pString = "";
	}
}

static void HolyLib_String_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	if (*pVar.m_pString == '\0')
	{
		// This also allows quick CalcDelta checking since if it's Zero it'll be a nullptr
		*((void**)pPackedData) = nullptr;
	} else {
		pState->AddString( pVar.m_pString, pProp->m_nNewTotalOffset );
	}
}

static bool HolyLib_Array_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	// Weh! I hate this though idk what to do rn
	return false;
}

static void HolyLib_Array_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	pVar.m_pData = nullptr; // Not used by Array encoding.
}

static void HolyLib_Array_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	DVariant pElementVar;
	int nType = ((HolyLibSendPropPrecalc*)pProp->GetArrayProp())->m_nHolyLibType;
	for (int nElement = 0; nElement < pProp->GetNumElements(); ++nElement)
	{
		pProp->GetProxyFn()(
			pProp,
			pState->m_pEntityBase,
			(char*)pState->m_pEntityBase + pProp->GetOffset(),
			&pElementVar,
			nElement,
			pProp->m_nPropID
		);
		
		pHolyLibPropTypeFns[nType].PackEntry(
			pProp,
			pPackedData + (pProp->m_nArrayElementSize * nElement),
			(pProp->m_nArrayElementSize * nElement),
			pElementVar,
			pState
		);
	}
}

static bool HolyLib_GMODTable_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	unsigned short numElements = *(unsigned short*)pPackedData;
	return numElements == 0;
}

static void HolyLib_GMODTable_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	// NOTE: GMOD Stores a void** pointer to the CGMODDataTable in the m_pData - See the GMODTable_Encode function
	pVar.m_pData = (void*)((const unsigned char*)pEntityData + pProp->GetOffset());
}

static void HolyLib_GMODTable_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	/*
		GMODTable struct:
		unsigned short(2 bytes) - numEntires
	*/

	unsigned short nNumEntries = 0;
	*(unsigned short*)pPackedData = nNumEntries;
}

static bool HolyLib_Bool_IsEncodedZero(const HolyLibSendPropPrecalc* pProp, const void* pPackedData)
{
	const uint8* pByte = (const uint8*)pPackedData + pProp->m_nNewTotalOffset;
	bool value = ((*pByte) >> pProp->m_nBitOffset) & 1;
	return !value;
}

static void HolyLib_Bool_FillDVariant(const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar)
{
	const uint8* pByte = (const uint8*)pPackedData + pProp->m_nNewTotalOffset;
	bool value = ((*pByte) >> pProp->m_nBitOffset) & 1;
	pVar.m_Int = value ? 1 : 0;
	Msg("Read value %s from %s at %p\n", value ? "true" : "false", pProp->GetName(), pPackedData);
}

static void HolyLib_Bool_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	*((uint8*)pPackedData) |= (pVar.m_Int == 1 ? 1 : 0) << pProp->m_nBitOffset;
	Msg("Wrote value %s from %s at %p\n", (pVar.m_Int == 1) ? "true" : "false", pProp->GetName(), pPackedData);
}

static bool HolyLib_Int24_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	int24* pVal = (int24*)pPackedData;
	return pVal->val[0] == 0 && pVal->val[1] == 0 && pVal->val[2] == 0;
}

static void HolyLib_Int24_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	int24* pVal = (int24*)pPackedData;
	pVar.m_Int = pVal->ToInt();
}

static void HolyLib_Int24_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	*(int24*)pPackedData = int24(pVar.m_Int);
}

static bool HolyLib_Int16_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	return (*(short*)pPackedData) == 0;
}

static void HolyLib_Int16_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	pVar.m_Int = *(short*)pPackedData;
}

static void HolyLib_Int16_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	*(short*)pPackedData = pVar.m_Int;
}

static bool HolyLib_Int8_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pPackedData )
{
	return (*(char*)pPackedData) == 0;
}

static void HolyLib_Int8_FillDVariant( const HolyLibSendPropPrecalc* pProp, const void* pPackedData, const void* pEntityData, DVariant& pVar )
{
	pVar.m_Int = *(char*)pPackedData;
}

static void HolyLib_Int8_PackEntry( const HolyLibSendPropPrecalc* pProp, unsigned char* pPackedData, int nCurrentOffset, const DVariant& pVar, PackState* pState )
{
	*(char*)pPackedData = pVar.m_Int;
}

HolyLibPropTypeFns pHolyLibPropTypeFns[HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes] = {
	{ // DPT_Int
		HolyLib_Int_IsEncodedZero,
		HolyLib_Int_FillDVariant,
		HolyLib_Int_PackEntry,
	},
	{ // DPT_Float
		HolyLib_Float_IsEncodedZero,
		HolyLib_Float_FillDVariant,
		HolyLib_Float_PackEntry,
	},
	{ // DPT_Vector
		HolyLib_Vector_IsEncodedZero,
		HolyLib_Vector_FillDVariant,
		HolyLib_Vector_PackEntry,
	},
	{ // DPT_VectorXY
		HolyLib_VectorXY_IsEncodedZero,
		HolyLib_VectorXY_FillDVariant,
		HolyLib_VectorXY_PackEntry,
	},
	{ // DPT_String
		HolyLib_String_IsEncodedZero,
		HolyLib_String_FillDVariant,
		HolyLib_String_PackEntry,
	},
	{ // DPT_Array
		HolyLib_Array_IsEncodedZero,
		HolyLib_Array_FillDVariant,
		HolyLib_Array_PackEntry,
	},
	{ // DPT_DataTable
		nullptr,
		nullptr,
		nullptr,
	},
	{ // DPT_GMODTable
		HolyLib_GMODTable_IsEncodedZero,
		HolyLib_GMODTable_FillDVariant,
		HolyLib_GMODTable_PackEntry,
	},
	{ // DPT_BOOL
		HolyLib_Bool_IsEncodedZero,
		HolyLib_Bool_FillDVariant,
		HolyLib_Bool_PackEntry,
	},
	{ // DPT_INT24
		HolyLib_Int24_IsEncodedZero,
		HolyLib_Int24_FillDVariant,
		HolyLib_Int24_PackEntry,
	},
	{ // DPT_INT16
		HolyLib_Int16_IsEncodedZero,
		HolyLib_Int16_FillDVariant,
		HolyLib_Int16_PackEntry,
	},
	{ // DPT_INT8
		HolyLib_Int8_IsEncodedZero,
		HolyLib_Int8_FillDVariant,
		HolyLib_Int8_PackEntry,
	},
};

/*
	Layout we generate:

	DPT_Int chunk
	DPT_Float chunk
	DPT_Vector chunk
	DPT_VectorXY chunk
	DPT_String pointer chunk - entires point to string data chunk - needed since these variable offsets need to be consistent! What they point at can be dynamic though
	DPT_Array chunk
	DPT_Bool chunk - bit flags are used
	DPT_INT24 chunk
	DPT_INT16 chunk
	DPT_INT8 chunk	
	StringData chunk - contains all strings

	Where do arrays go? Their entires were added to their given types - so not needed. Simplifies things :3
*/

int PackState::BuildPack()
{
	DVariant pVar;
	int iNumProps = m_pPrecalc->GetNumProps();
	for (int iProp = 0; iProp < iNumProps; ++iProp)
	{
		const HolyLibSendPropPrecalc* pProp = (HolyLibSendPropPrecalc*)m_pPrecalc->GetProp(iProp);

		pProp->GetProxyFn()(
			pProp,
			m_pEntityBase,
			(char*)m_pEntityBase + pProp->GetOffset(),
			&pVar,
			0,
			pProp->m_nPropID
		);

		pHolyLibPropTypeFns[pProp->m_nHolyLibType].PackEntry(
			pProp,
			m_pPackedData + pProp->m_nNewTotalOffset,
			pProp->m_nNewTotalOffset,
			pVar,
			this
		);
	}

	int nTotalSize = m_pPrecalc->m_nSendPropDataSize;
	// Written at the end since size is dynamic / can change while everything above is static in size
	for (StringEntry& pEntry : pStringEnties)
	{
		int nLength = strlen(pEntry.pString);
		if (nLength == 0) // Should normally not even happen, though you can never be 100% sure xd
		{
			*((void**)((char*)m_pPackedData + pEntry.nDataOffset)) = nullptr;
		} else {
			unsigned char* pEntryOffset = m_pPackedData;
			*((void**)((char*)pEntryOffset + pEntry.nDataOffset)) = (void*)pEntryOffset;
			memcpy(pEntryOffset, pEntry.pString, nLength);
			pEntryOffset[nLength] = '\0';
			// Msg(PROJECT_NAME " - Wrote string %s (%i)\n", pEntry.pString, nLength);
			nTotalSize += nLength;
		}
	}

	return nTotalSize;
}

class EncodeState
{
public:
	EncodeState( const void* pStructBase, const void* pPackedStructBase, bf_write* pBuf )
	{
		m_pBuf = pBuf;
		m_pStructBase = (const unsigned char*)pStructBase;
		m_pPackedStructBase = (const unsigned char*)pPackedStructBase;
	}

	void WritePropIndex( int iProp );

	int m_iLastProp = -1;
	bf_write* m_pBuf = nullptr;
	const unsigned char* m_pStructBase = nullptr;
	const unsigned char* m_pPackedStructBase = nullptr;
};

FORCEINLINE void EncodeState::WritePropIndex( int iProp ) // Yoinked from CDeltaBitsWriter
{
	Assert( iProp >= 0 && iProp < MAX_DATATABLE_PROPS );
	unsigned int diff = iProp - m_iLastProp;
	m_iLastProp = iProp;
	Assert( diff > 0 && diff <= MAX_DATATABLE_PROPS );
	// Expanded inline for maximum efficiency.
	//m_pBuf->WriteOneBit( 1 );
	//m_pBuf->WriteUBitVar( diff - 1 );
	COMPILE_TIME_ASSERT( MAX_DATATABLE_PROPS <= 0x1000u );
	int n = ((diff < 0x11u) ? -1 : 0) + ((diff < 0x101u) ? -1 : 0);
	m_pBuf->WriteUBitLong( diff*8 - 8 + 4 + n*2 + 1, 8 + n*4 + 4 + 2 + 1 );
}

static PropTypeFns pPropTypeFns[DPT_NUMSendPropTypes];
// This is special - why? Because we DON'T pull data from the entity, instead we use the packed data which already was passed through the proxies.
// This allows us to possibly encode snapshots from the past as the source engine normally can't do this (the proxies would return different results)
// BUG: This is only partially true - due to the GMODTable and how it's not really saved / fked up we can only partially do this.
static void HolyLib_SendTable_EncodeProp( EncodeState* pState, const HolyLibSendPropPrecalc* pProp, int iProp )
{
	DVariant var;
	pHolyLibPropTypeFns[pProp->m_nHolyLibType].FillDVariant(
		pProp,
		pState->m_pPackedStructBase + pProp->m_nNewTotalOffset,
		pState->m_pStructBase,
		var
	);

	DVariant var2;
	pProp->GetProxyFn()(
		pProp,
		pState->m_pStructBase,
		(char*)pState->m_pStructBase + pProp->GetOffset(),
		&var2,
		0,
		pProp->m_nPropID
	);

	var.m_Type = pProp->m_Type;
	var2.m_Type = pProp->m_Type;
	std::string varStr = var.ToString(); // Yes .ToString can only be used one at a time
	if ( V_stricmp(varStr.c_str(), var2.ToString() ) != 0)
		Msg("%s (%i) - %s | %s\n", pProp->GetName(), pProp->m_nHolyLibType, varStr.c_str(), var2.ToString());

	// Write the index.
	pState->WritePropIndex( iProp );

	g_PropTypeFns[pProp->m_Type].Encode( 
		pState->m_pStructBase, 
		&var, 
		pProp, 
		pState->m_pBuf, 
		pProp->m_nPropID
	); 
}

static bool HolyLib_SendTable_Encode(const SendTable *pTable,
	const void *pStruct,
	const void *pPackedStruct,
	bf_write *pOut, 
	int objectID,
	CUtlMemory<CSendProxyRecipients> *pRecipients,
	bool bNonZeroOnly,
	bool bBaselineEncode
	// Used for the GMODTable to skip encoding.
	// Why? Because if they get written into the baseline the client will shit itself.
	// This is because of the very dynamic nature of it and having it written into the base line
	// would cause all the present NW2Vars to be applied to all entities of the class
)
{
	CSendTablePrecalc *pPrecalc = pTable->m_pPrecalc;
	if (!pPrecalc)
		Error("SendTable_Encode: Missing m_pPrecalc for SendTable %s.", pTable->m_pNetTableName);

	if (!pRecipients || pRecipients->NumAllocated() < pPrecalc->GetNumDataTableProxies())
		Error("SendTable_Encode: pRecipients array too small. (%p - %i/%i)", pRecipients, pRecipients ? pRecipients->NumAllocated() : -1, pPrecalc->GetNumDataTableProxies());

	EncodeState pState(pStruct, pPackedStruct, pOut);

	int iNumProps = pPrecalc->GetNumProps();
	for (int iProp = 0; iProp < iNumProps; ++iProp)
	{
		// Yes this is valid - we replaced entries.
		const HolyLibSendPropPrecalc* pProp = (HolyLibSendPropPrecalc*)pPrecalc->GetProp(iProp);

		HolyLib_SendTable_EncodeProp( &pState, pProp, iProp );
	}

	return true;
}

static int HolyLib_SendTable_CalcDelta( // For OUR data input! Not the compressed - our RAW
	const SendTable *pTable,
	
	const void *pFromState,
	const int nFromBytes,

	const unsigned char *pToState,
	const int nToBytes,
	
	int *pDeltaProps,
	int nMaxDeltaProps,

	const int objectID) 
{
	int* pDeltaPropsBase = pDeltaProps;
	int* pDeltaPropsEnd = pDeltaProps + nMaxDeltaProps;

	// Should be fine since we replaced all Precalc tables. (even the nested ones)
	HolyLibCSendTablePrecalc* pPrecalc = (HolyLibCSendTablePrecalc*)pTable->m_pPrecalc;

	if ( pFromState )
	{

	} else {
		int iNumProps = pPrecalc->GetNumProps();
		for (int iProp = 0; iProp < iNumProps; ++iProp)
		{
			const HolyLibSendPropPrecalc* pProp = (HolyLibSendPropPrecalc*)pPrecalc->GetProp(iProp);

			pHolyLibPropTypeFns[pProp->m_nHolyLibType].IsEncodedZero(
				pProp,
				pToState + pProp->m_nNewTotalOffset
			);
		}
	}

	return 0;
}

#if MODULE_EXISTS_NETWORKING
extern IChangeFrameList* hook_AllocChangeFrameList(int nProperties, int iCurTick);
#endif

#if MODULE_EXISTS_SOURCETV
extern CHLTVServer* hltv;
#endif

// Dependency on networking module for now until this is working after which I can remove this dependency.
bool g_bRedirectPackEntity = false;
static CFrameSnapshotManager* framesnapshotmanager = nullptr;
static Symbols::SV_EnsureInstanceBaseline func_SV_EnsureInstanceBaseline = nullptr;
static Symbols::CFrameSnapshotManager_CreatePackedEntity func_CFrameSnapshotManager_CreatePackedEntity = nullptr;
static Symbols::CFrameSnapshotManager_GetPreviouslySentPacket func_CFrameSnapshotManager_GetPreviouslySentPacket = nullptr;
static Symbols::CFrameSnapshotManager_UsePreviouslySentPacket func_CFrameSnapshotManager_UsePreviouslySentPacket = nullptr;
void NWR_SV_PackEntity(int edictIdx, edict_t* edict, ServerClass* pServerClass, CFrameSnapshot *pSnapshot)
{
	Assert( edictIdx < pSnapshot->m_nNumEntities );
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "PackEntities_Normal%s", __FUNCTION__ );

	int iSerialNum = pSnapshot->m_pEntities[ edictIdx ].m_nSerialNumber;
	
	// First encode the entity's data.
	ALIGN4 char packedData[MAX_PACKEDENTITY_DATA] ALIGN4_POST;
	memset(packedData, 0, sizeof(packedData)); // We currently depend on it bering zerod

	SendTable *pSendTable = pServerClass->m_pTable;
	HolyLibCSendTablePrecalc *pSendTablePrecalc = (HolyLibCSendTablePrecalc*)pSendTable->m_pPrecalc;

	unsigned char tempData[ sizeof( CSendProxyRecipients ) * MAX_DATATABLE_PROXIES ];
	CUtlMemory< CSendProxyRecipients > recip( (CSendProxyRecipients*)tempData, pSendTablePrecalc->GetNumDataTableProxies() );

	Msg("Edict: %p\n", edict);
	Msg("Entity: %p\n", edict->GetUnknown());

	int nWrittenBytes = SendProp_FillSnapshot( pSendTablePrecalc, packedData, edict->GetUnknown() );
	if ( nWrittenBytes > sizeof(packedData) )
		Error(PROJECT_NAME " - SV_PackEntity: Out of memory write! Packed entity overflowed!\n");

	if (pServerClass->m_InstanceBaselineIndex == INVALID_STRING_INDEX)
	{
		/*
			What exactly is happening here?
			This needs some explaining just for a better overview.
			We use a stringtable in which we store the baseline encoded data for a entity class.
			This is used in decoding to compare against? - this also means that anything the first entity contained is considered the baseline.
			This sometimes can cause issues if something wasn't implemented properly (looking at you NW2/GMODDataTables)

			I'll probably come back to this to adjust since I still don't fully understand it yet / may be wrong.
		*/
		ALIGN4 char writeBuffer[MAX_PACKEDENTITY_DATA] ALIGN4_POST;
		bf_write writeBuf( "SV_PackEntity->writeBuf", writeBuffer, sizeof( writeBuffer ) );
		if( !HolyLib_SendTable_Encode( pSendTable, edict->GetUnknown(), packedData, &writeBuf, edictIdx, &recip, false, true ) )						 
			Error( "SV_PackEntity: SendTable_Encode returned false (ent %d).\n", edictIdx );

		func_SV_EnsureInstanceBaseline( pServerClass, edictIdx, writeBuffer, writeBuf.GetNumBytesWritten() );	
	}

	g_pFullFileSystem->CreateDirHierarchy("holylib/dump/newdt/", "MOD");
	std::string fileName = "holylib/dump/newdt/";
	fileName.append(pServerClass->GetName());
	fileName.append(".dt");

	FileHandle_t pPackDump = g_pFullFileSystem->Open(fileName.c_str(), "wb", "MOD");
	if (pPackDump)
	{
		g_pFullFileSystem->Write(packedData, sizeof(packedData), pPackDump);
		g_pFullFileSystem->Close(pPackDump);
	}
	
	int nFlatProps = pSendTablePrecalc->GetNumProps();
	IChangeFrameList *pChangeFrame = nullptr;

	// GMOD - m_nGMODDataTableOffset only exists on dev & 64x NOT MAIN till next update! See: https://github.com/Facepunch/garrysmod-requests/issues/2981
	CGMODDataTable* pGMODDataTable = nullptr;
	int nGMODDataTableOffset = pSendTablePrecalc->m_SendPropStruct->m_nGModDataTableProp->GetOffset(); // Since m_nGMODDataTableOffset isn't available
	if ( nGMODDataTableOffset != -1 ) // We use a precalculated offset, since finding it in here is uttelry expensive!
		pGMODDataTable = *(CGMODDataTable**)((char*)edict->GetUnknown() + nGMODDataTableOffset);

#if 0
	// If this entity was previously in there, then it should have a valid IChangeFrameList 
	// which we can delta against to figure out which properties have changed.
	//
	// If not, then we want to setup a new IChangeFrameList.
	PackedEntity *pPrevFrame = func_CFrameSnapshotManager_GetPreviouslySentPacket( framesnapshotmanager, edictIdx, pSnapshot->m_pEntities[ edictIdx ].m_nSerialNumber );
	if ( pPrevFrame )
	{
		// Calculate a delta.
		Assert( !pPrevFrame->IsCompressed() );
		
		int deltaProps[MAX_DATATABLE_PROPS];

		int nChanges = HolyLib_SendTable_CalcDelta(
			pSendTable, 
			pPrevFrame->GetData(), pPrevFrame->GetNumBytes(),
			packedData,	nWrittenBytes,
			
			deltaProps,
			ARRAYSIZE( deltaProps ),

			edictIdx
			);

		// If it's non-manual-mode, but we detect that there are no changes here, then just
		// use the previous pSnapshot if it's available (as though the entity were manual mode).
		// It would be interesting to hook here and see how many non-manual-mode entities 
		// are winding up with no changes.
		if ( nChanges == 0 )
		{
			if ( pPrevFrame->CompareRecipients( recip ) )
			{
				if ( func_CFrameSnapshotManager_UsePreviouslySentPacket( framesnapshotmanager, pSnapshot, edictIdx, iSerialNum ) )
				{
					edict->ClearStateChanged();
					return;
				}
			}
		}
		else
		{
			if ( !edict->HasStateChanged() )
			{
				for ( int iDeltaProp=0; iDeltaProp < nChanges; iDeltaProp++ )
				{
					Assert( pSendTable->m_pPrecalc );
					Assert( deltaProps[iDeltaProp] < pSendTable->m_pPrecalc->GetNumProps() );

					const SendProp *pProp = pSendTable->m_pPrecalc->GetProp( deltaProps[iDeltaProp] );
					// If a field changed, but it changed because it encoded against tickcount, 
					//   then it's just like the entity changed the underlying field, not an error, that is.
					if ( pProp->GetFlags() & SPROP_ENCODED_AGAINST_TICKCOUNT )
						continue;

					Msg( "Entity %d (class '%s') reported ENTITY_CHANGE_NONE but '%s' changed.\n", 
						edictIdx,
						edict->GetClassName(),
						pProp->GetName() );
				}
			}
		}

		if ( hltv && hltv->IsActive() )
		{
			// in HLTV or Replay mode every PackedEntity keeps it's own ChangeFrameList
			// we just copy the ChangeFrameList from prev frame and update it
			pChangeFrame = pPrevFrame->GetChangeFrameList();
			pChangeFrame = pChangeFrame->Copy(); // allocs and copies ChangeFrameList
		}
		else
		{
			// Ok, now snag the changeframe from the previous frame and update the 'last frame changed'
			// for the properties in the delta.
			pChangeFrame = pPrevFrame->SnagChangeFrameList();
		}
		
		ErrorIfNot( pChangeFrame, ("SV_PackEntity: SnagChangeFrameList returned null") );
		ErrorIfNot( pChangeFrame->GetNumProps() == nFlatProps, ("SV_PackEntity: SnagChangeFrameList mismatched number of props[%d vs %d]", nFlatProps, pChangeFrame->GetNumProps() ) );

		pChangeFrame->SetChangeTick( deltaProps, nChanges, pSnapshot->m_nTickCount );
	}
	else
	{
		// Ok, init the change frames for the first time.
#if MODULE_EXISTS_NETWORKING // the change list from the networking module is way more efficient
		pChangeFrame = hook_AllocChangeFrameList( nFlatProps, pSnapshot->m_nTickCount );
#else
		pChangeFrame = AllocChangeFrameList( nFlatProps, pSnapshot->m_nTickCount );
#endif
	}

	// Now make a PackedEntity and store the new packed data in there.
	PackedEntity *pPackedEntity = func_CFrameSnapshotManager_CreatePackedEntity( framesnapshotmanager, pSnapshot, edictIdx );
	pPackedEntity->SetChangeFrameList( pChangeFrame );
	pPackedEntity->SetServerAndClientClass( pServerClass, nullptr );
	pPackedEntity->AllocAndCopyPadded( packedData, nWrittenBytes );
	pPackedEntity->SetRecipients( recip );
	pPackedEntity->m_pGModDataTable = pGMODDataTable;

	edict->ClearStateChanged();
#endif
}

/*
	Replacement part of CSendTablePrecalc with HolyLibCSendTablePrecalc into gmod
*/
static std::unordered_map<CSendTablePrecalc*, HolyLibCSendTablePrecalc*> g_pReplacedProps;
static HolyLibCSendTablePrecalc* AllocateNewPrecalc(CSendTablePrecalc* pPrecalc)
{
	if (!pPrecalc)
		return nullptr;

	auto it = g_pReplacedProps.find(pPrecalc);
	if (it != g_pReplacedProps.end())
		return it->second;

	HolyLibCSendTablePrecalc* pNew = new HolyLibCSendTablePrecalc;

	// Inherit all values.
	// But wait... isn't this unsafe?
	// Yes it very much is. But since at this point their static/nothing should change and should be read only were fine.
	memcpy(pNew, pPrecalc, sizeof(CSendTablePrecalc));

	pNew->PrecalcSendProps();

	g_pReplacedProps[pPrecalc] = pNew;

	return pNew;
}

extern void NWR_AddSendProp(SendProp* pProp);
extern void NWR_AddSendTable(SendTable* pTables);
void NWR_AddSendProp(SendProp* pProp)
{
	if (pProp->GetDataTable())
		NWR_AddSendTable(pProp->GetDataTable());

	if (pProp->GetArrayProp())
		NWR_AddSendProp(pProp->GetArrayProp());
}

void NWR_AddSendTable(SendTable* pTable)
{
	for (int i = 0; i < pTable->GetNumProps(); i++)
		NWR_AddSendProp(&pTable->m_pProps[i]); // Windows screwing with GetProp

	pTable->m_pPrecalc = (HolyLibCSendTablePrecalc*)AllocateNewPrecalc(pTable->m_pPrecalc);
}

static HolyLibSendPropPrecalc* g_pHeadProp = nullptr;
void CNetworkingReplacementModule::ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
	if (g_pHeadProp)
		return;

	Msg(PROJECT_NAME " - networkingreplacement: Injecting new SendPropPrecalc classes. Unloading this module is now unstable/should not be done!\n");

	for(ServerClass *serverclass = Util::servergamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext)
		NWR_AddSendTable(serverclass->m_pTable);

	g_bRedirectPackEntity = true;
}

/*
	Detouring
*/

void CNetworkingReplacementModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;
	
	SourceSDK::FactoryLoader engine_loader("engine");
	func_SV_EnsureInstanceBaseline = (Symbols::SV_EnsureInstanceBaseline)Detour::GetFunction(engine_loader.GetModule(), Symbols::SV_EnsureInstanceBaselineSym);
	Detour::CheckFunction((void*)func_SV_EnsureInstanceBaseline, "SV_EnsureInstanceBaseline");

#if defined(ARCHITECTURE_X86) && defined(SYSTEM_LINUX)
	PropTypeFns* pPropTypeFns = Detour::ResolveSymbol<PropTypeFns>(engine_loader, Symbols::g_PropTypeFnsSym);
#else
	PropTypeFns* pPropTypeFns = Detour::ResolveSymbolWithOffset<PropTypeFns>(engine_loader.GetModule(), Symbols::g_PropTypeFnsSym);
#endif
	Detour::CheckValue("get class", "pPropTypeFns", pPropTypeFns != nullptr);

	if (pPropTypeFns)
	{
		for (size_t i = 0; i < DPT_NUMSendPropTypes; ++i)
			g_PropTypeFns[i] = pPropTypeFns[i]; // Crash any% speed run. I don't believe this will work... It does... work...
	}
}