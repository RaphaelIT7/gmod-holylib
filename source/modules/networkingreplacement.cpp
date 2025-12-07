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
	m_pDTITable = NULL;
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

	int m_nNewOffset = 0; // We inherit m_Offset already, this is to map the old offset to our new one!
	int m_nNewSize = 0; // Size in our new struct - in bytes! (if -1 then it's a bool!)
	int m_nBitOffset = 0; // For bool types since we pack them into a byte
	unsigned short m_nPropID = 0;
	unsigned char m_nArrayElementSize = 0;
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

	CUtlVector<const SendProp*> pProps[HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes];
	for (int i = 0; i < GetNumProps(); ++i)
	{
		const SendProp* pProp = GetProp(i);
		SendPropType type = pProp->GetType();
    
		if (type >= 0 && type < DPT_NUMSendPropTypes)
		{
			int nBytes = ((pProp->m_nBits + 7) & ~7) / 8;
			if (type == SendPropType::DPT_Int && pProp->m_nBits == 1)
			{
				pProps[HolyLibSendPropPrecalc::DPT_BOOL].AddToTail(pProp);
			} else if (type == SendPropType::DPT_Int && nBytes == 1)
			{
				pProps[HolyLibSendPropPrecalc::DPT_INT8].AddToTail(pProp);
			} else if (type == SendPropType::DPT_Int && nBytes == 2)
			{
				pProps[HolyLibSendPropPrecalc::DPT_INT16].AddToTail(pProp);
			} else if (type == SendPropType::DPT_Int && nBytes == 3)
			{
				pProps[HolyLibSendPropPrecalc::DPT_INT24].AddToTail(pProp);
			} else {
				pProps[type].AddToTail(pProp);
			}
		} else {
			Warning(PROJECT_NAME " - HolyLibCSendTablePrecalc::PrecalcSendProps %s has invalid type %d\n", pProp->GetName(), type);
		}
	}

	int nPropID = -1; // Were using preincrement!
	for (int nDPTType = 0; nDPTType < HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes; ++nDPTType)
	{
		SendPropStruct& pStruct = m_SendPropStruct[nDPTType];
		pStruct.m_nBits = 0;
		pStruct.m_nBytes = 0;

		FOR_EACH_VEC( pProps[nDPTType], i )
		{
			const SendProp* pProp = pProps[nDPTType][i];
			int nBits = pProp->m_nBits;
			if (nBits <= 0)
			{
				switch(nDPTType)
				{
				case SendPropType::DPT_Int:
					nBits = sizeof(int) * 8;
					break;
				case SendPropType::DPT_Float:
					nBits = sizeof(float) * 8;
					break;
				case SendPropType::DPT_Vector:
					nBits = sizeof(float) * 8 * 3;
					break;
				case SendPropType::DPT_Array:
					nBits = 0; // How will I handle arrays? I don't know yet...
					break;
				case SendPropType::DPT_VectorXY:
					nBits = sizeof(float) * 8 * 2;
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
				case SendPropType::DPT_GMODTable:
					break;
				default:
					Warning(PROJECT_NAME " - HolyLibCSendTablePrecalc::PrecalcSendProps: No idea for size of %s!\n", pProp->GetName());
					break;
				}
			}

			HolyLibSendPropPrecalc* pPrecalc = new HolyLibSendPropPrecalc;
			memcpy(pPrecalc, pProp, sizeof(SendProp));
			pPrecalc->m_nPropID = ++nPropID;
			pPrecalc->m_Type = (SendPropType)nDPTType; // Just to make the compiler happy...

			if (nDPTType == HolyLibSendPropPrecalc::DPT_BOOL) // Bool, yay, we store them in bits :3
			{
				if (pStruct.m_nBytes == 0) // Initial size! Needed for later allocation as else we'd have to check the bits
					++pStruct.m_nBytes;

				pPrecalc->m_nNewOffset = pStruct.m_nBytes-1; // -1 since m_nBytes will always be +1 for the current byte
				pPrecalc->m_nNewSize = HolyLibSendPropPrecalc::BOOL_SIZE;
				pPrecalc->m_nBitOffset = pStruct.m_nBits++;
				if (pStruct.m_nBits >= 8) // not >= since else we'd skip the 8th bit
				{
					++pStruct.m_nBytes;
					pStruct.m_nBits = 0;
				}

				pStruct.m_pProps.AddToTail(pPrecalc);
			} else if (nDPTType == SendPropType::DPT_GMODTable) {
				pStruct.m_nGModDataTableProp = pPrecalc;
			} else if (nDPTType == SendPropType::DPT_Array) {
				SendProp* pArrayProp = pProp->GetArrayProp();

				pPrecalc->m_Offset = pArrayProp->GetOffset(); // Inherit offset
				pPrecalc->m_nNewOffset = pStruct.m_nBytes;

				pPrecalc->m_nArrayElementSize = ((pArrayProp->m_nBits + 7) & ~7) / 8;
				int nBytes = pPrecalc->GetNumElements() * pPrecalc->m_nArrayElementSize;
				pPrecalc->m_nNewSize = nBytes;

				if (pArrayProp->m_nBits == 1 && pArrayProp->m_Type == DPT_Int)
					Error("Array %s is bool?\n", pArrayProp->GetName()); // Currently this shouldn't be the case - if it becomes the case I'll need to rework some shit

				pStruct.m_nBytes += nBytes;
				pStruct.m_pProps.AddToTail(pPrecalc);
			} else {
				pPrecalc->m_nNewOffset = pStruct.m_nBytes;

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

struct int24
{
	int ToInt() const
	{
		int value = (val[0]) | (val[1] << 8 ) | (val[2] << 16);

		if (value & 0x00800000)
			value |= 0xFF000000;

		return value;
	}

	char val[3];
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
static int SendProp_FillSnapshot( HolyLibCSendTablePrecalc *pTable, void* pOutData, const void* pEntity )
{
	Msg("SendProp_FillSnapshot 1\n");
	struct StringEntry
	{
		const char* pString;
		int nDataOffset; // To update the data pack entry
	};
	std::vector<StringEntry> pStringEnties;

	int nCurrentByte = 0;
	for (int nDPTType = 0; nDPTType < HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes; ++nDPTType)
	{
		const HolyLibCSendTablePrecalc::SendPropStruct& pStruct = pTable->m_SendPropStruct[nDPTType];

		if (nDPTType == HolyLibSendPropPrecalc::DPT_BOOL)
		{
			memset((char*)pOutData + nCurrentByte, 0, pStruct.m_nBytes); // We use bit flags so we better ensure it starts with 0!
			FOR_EACH_VEC( pStruct.m_pProps, i )
			{
				const HolyLibSendPropPrecalc* pProp = pStruct.m_pProps[i];
				DVariant pVar;
				pProp->GetProxyFn()(pProp, pEntity, (char*)pEntity + pProp->GetOffset(), &pVar, 0, pProp->m_nPropID);

				*((uint8*)((char*)pOutData + nCurrentByte + pProp->m_nNewOffset)) |= (pVar.m_Int ? 1 : 0) << pProp->m_nBitOffset;
				Msg(PROJECT_NAME " - Wrote %i into %i (%s) (%s)\n", nDPTType, nCurrentByte, pVar.m_Int ? "true" : "false", pProp->GetName());
			}

			nCurrentByte += pStruct.m_nBytes;
		} else if (nDPTType == SendPropType::DPT_String) { // A bit expensive yet fine since there are only a few
			FOR_EACH_VEC( pStruct.m_pProps, i )
			{
				const HolyLibSendPropPrecalc* pProp = pStruct.m_pProps[i];
				DVariant pVar;
				pProp->GetProxyFn()(pProp, pEntity, (char*)pEntity + pProp->GetOffset(), &pVar, 0, pProp->m_nPropID);
				if (*pVar.m_pString == '\0')
				{
					// This also allows quick CalcDelta checking since if it's Zero it'll be a nullptr
					*((void**)((char*)pOutData + nCurrentByte)) = nullptr;
				} else {
					StringEntry& pEntry = pStringEnties.emplace_back();
					pEntry.nDataOffset = nCurrentByte;
					pEntry.pString = pVar.m_pString;
				}
				Msg(PROJECT_NAME " - Wrote %i into %i (%s) (%s)\n", nDPTType, nCurrentByte, pVar.m_pString, pProp->GetName());
				nCurrentByte += pProp->m_nNewSize;
			}
		} else if (nDPTType == SendPropType::DPT_Array) {
			FOR_EACH_VEC( pStruct.m_pProps, i )
			{
				const HolyLibSendPropPrecalc* pProp = pStruct.m_pProps[i];

				DVariant pVar;
				for (int nElement = 0; nElement < pProp->GetNumElements(); ++nElement)
				{
					pProp->GetProxyFn()(pProp, pEntity, (char*)pEntity + pProp->GetOffset(), &pVar, 0, pProp->m_nPropID);
				
					memcpy((char*)pOutData + nCurrentByte, &pVar, pProp->m_nNewSize);
					nCurrentByte += pProp->m_nArrayElementSize;
				}

				Msg(PROJECT_NAME " - Wrote %i into %i (bytes: %i) (%s)\n", nDPTType, nCurrentByte, pProp->GetNumElements() * pProp->m_nArrayElementSize, pProp->GetName());
			}
		} else {
			FOR_EACH_VEC( pStruct.m_pProps, i )
			{
				const HolyLibSendPropPrecalc* pProp = pStruct.m_pProps[i];
				DVariant pVar;
				pProp->GetProxyFn()(pProp, pEntity, (char*)pEntity + pProp->GetOffset(), &pVar, 0, pProp->m_nPropID);
				
				memcpy((char*)pOutData + nCurrentByte, &pVar, pProp->m_nNewSize);
				if (pProp->m_Type == HolyLibSendPropPrecalc::DPT_INT8 || pProp->m_Type == HolyLibSendPropPrecalc::DPT_INT16 || pProp->m_Type == HolyLibSendPropPrecalc::DPT_INT24)
					pVar.m_Type = DPT_Int;
				else
					pVar.m_Type = pProp->m_Type;

				Msg(PROJECT_NAME " - Wrote %i into %i (%i) - %s (%s)\n", nDPTType, nCurrentByte, pProp->m_nNewSize, pVar.ToString(), pProp->GetName());
				nCurrentByte += pProp->m_nNewSize;
			}
		}
	}

	// Written at the end since size is dynamic / can change while everything above is static in size
	for (StringEntry& pEntry : pStringEnties)
	{
		int nLength = strlen(pEntry.pString);
		if (nLength == 0) // Should normally not even happen, though you can never be 100% sure xd
		{
			*((void**)((char*)pOutData + pEntry.nDataOffset)) = nullptr;
		} else {
			char* pEntryOffset = (char*)pOutData + nCurrentByte;
			*((void**)((char*)pOutData + pEntry.nDataOffset)) = (void*)pEntryOffset;
			memcpy(pEntryOffset, pEntry.pString, nLength);
			pEntryOffset[nLength] = '\0';
			nCurrentByte += nLength + 1;
			Msg(PROJECT_NAME " - Wrote string %s (%i)\n", pEntry.pString, nLength);
		}
	}

	Msg("SendProp_FillSnapshot 2\n");
	return nCurrentByte;
}

bool HolyLib_Int_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	return (*(int*)pData) == 0;
}

bool HolyLib_Float_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	return (*(float*)pData) == 0;
}

bool HolyLib_Vector_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	Vector* pVec = (Vector*)pData;
	return pVec->x == 0 && pVec->y == 0 && pVec->z == 0;
}

bool HolyLib_VectorXY_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	Vector* pVec = (Vector*)pData;
	return pVec->x == 0 && pVec->y == 0;
}

bool HolyLib_String_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	// SPECIAL: Since we store a void* pointer IF the string has data we can check if it's null
	return *(void**)pData == nullptr;
}

bool HolyLib_Array_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	// Weh! I hate this though idk what to do rn
	return false;
}

bool HolyLib_Int24_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	int24* pVal = (int24*)pData;
	return pVal->val[0] == 0 && pVal->val[1] == 0 && pVal->val[2] == 0;
}

bool HolyLib_Int16_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	return (*(short*)pData) == 0;
}

bool HolyLib_Int8_IsEncodedZero( const HolyLibSendPropPrecalc* pProp, const void* pData )
{
	return (*(char*)pData) == 0;
}

typedef struct
{
	bool			(*IsEncodedZero) ( const HolyLibSendPropPrecalc* pProp, const void* pData );
} HolyLibPropTypeFns;

static HolyLibPropTypeFns pHolyLibPropTypeFns[HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes] = {
	{ // DPT_Int
		HolyLib_Int_IsEncodedZero,
	},
	{ // DPT_Float
		HolyLib_Float_IsEncodedZero,
	},
	{ // DPT_Vector
		HolyLib_Vector_IsEncodedZero,
	},
	{ // DPT_VectorXY
		HolyLib_VectorXY_IsEncodedZero,
	},
	{ // DPT_String
		HolyLib_String_IsEncodedZero,
	},
	{ // DPT_Array
		NULL,
	},
	{ // DPT_DataTable
		NULL,
	},
	{ // DPT_GMODTable
		NULL,
	},
	{ // DPT_BOOL
		NULL,
	},
	{ // DPT_INT24
		NULL,
	},
	{ // DPT_INT16
		NULL,
	},
	{ // DPT_INT8
		NULL,
	},
};

static PropTypeFns pPropTypeFns[DPT_NUMSendPropTypes];
int HolyLib_SendTable_CalcDelta( // For OUR data input! Not the compressed - our RAW
	const SendTable *pTable,
	
	const void *pFromState,
	const int nFromBytes,

	const void *pToState,
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
		int nCurrentByte = 0;
		for (int nDPTType = 0; nDPTType < HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes; ++nDPTType)
		{
			const HolyLibCSendTablePrecalc::SendPropStruct& pStruct = pPrecalc->m_SendPropStruct[nDPTType];

			if (nDPTType == HolyLibSendPropPrecalc::DPT_BOOL)
			{
				FOR_EACH_VEC( pStruct.m_pProps, i )
				{
					const HolyLibSendPropPrecalc* pProp = pStruct.m_pProps[i];
				}

				nCurrentByte += pStruct.m_nBytes;
			} else {
				FOR_EACH_VEC( pStruct.m_pProps, iToProp )
				{
					const HolyLibSendPropPrecalc* pProp = pStruct.m_pProps[iToProp];
					if (pHolyLibPropTypeFns[pProp->m_Type].IsEncodedZero(pProp, (char*)pToState + nCurrentByte))
					{
						*pDeltaProps++ = iToProp;
						if ( pDeltaProps >= pDeltaPropsEnd )
							break;
					}
				}
			}
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
static Symbols::SendTable_Encode func_SendTable_Encode = nullptr;
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

	SendTable *pSendTable = pServerClass->m_pTable;

	unsigned char tempData[ sizeof( CSendProxyRecipients ) * MAX_DATATABLE_PROXIES ];
	CUtlMemory< CSendProxyRecipients > recip( (CSendProxyRecipients*)tempData, pSendTable->m_pPrecalc->GetNumDataTableProxies() );

	Msg("Edict: %p\n", edict);
	Msg("Entity: %p\n", edict->GetUnknown());
	Msg("NWR_SV_PackEntity 1\n");
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
		bf_write writeBuf( "SV_PackEntity->writeBuf", packedData, sizeof( packedData ) );
		if( !func_SendTable_Encode( pSendTable, edict->GetUnknown(), &writeBuf, edictIdx, &recip, false ) )						 
			Error( "SV_PackEntity: SendTable_Encode returned false (ent %d).\n", edictIdx );

		//func_SV_EnsureInstanceBaseline( pServerClass, edictIdx, packedData, writeBuf.GetNumBytesWritten() );	
	}

	Msg("NWR_SV_PackEntity 2\n");
	// SendProp_FillSnapshot( (HolyLibCSendTablePrecalc*)pSendTable->m_pPrecalc, packedData, edict->GetUnknown() );

	Msg("NWR_SV_PackEntity 3\n");
	g_pFullFileSystem->CreateDirHierarchy("holylib/dump/newdt/", "MOD");
	std::string fileName = "holylib/dump/newdt/";
	fileName.append(pServerClass->GetName());
	fileName.append(".dt");

	Msg("NWR_SV_PackEntity 4\n");
	FileHandle_t pPackDump = g_pFullFileSystem->Open(fileName.c_str(), "wb", "MOD");
	if (pPackDump)
	{
		g_pFullFileSystem->Write(packedData, sizeof(packedData), pPackDump);
		g_pFullFileSystem->Close(pPackDump);
	}
	
	Msg("NWR_SV_PackEntity 5\n");
	#if 0
	int nFlatProps = SendTable_GetNumFlatProps( pSendTable );
	IChangeFrameList *pChangeFrame = NULL;

	// GMOD - m_nGMODDataTableOffset only exists on dev & 64x NOT MAIN till next update! See: https://github.com/Facepunch/garrysmod-requests/issues/2981
	CGMODDataTable* pGMODDataTable = NULL;
	int nGMODDataTableOffset = pSendTable->m_pPrecalc->m_nGMODDataTableOffset;
	if ( nGMODDataTableOffset != -1 ) // We use a precalculated offset, since finding it in here is uttelry expensive!
		pGMODDataTable = *(CGMODDataTable**)((char*)edict->GetUnknown() + nGMODDataTableOffset);

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

		int nChanges = SendTable_CalcDelta(
			pSendTable, 
			pPrevFrame->GetData(), pPrevFrame->GetNumBits(),
			packedData,	writeBuf.GetNumBitsWritten(),
			
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
	pPackedEntity->SetServerAndClientClass( pServerClass, NULL );
	pPackedEntity->AllocAndCopyPadded( packedData, writeBuf.GetNumBytesWritten() );
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

	//pNew->PrecalcSendProps();

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

	//for(ServerClass *serverclass = Util::servergamedll->GetAllServerClasses(); serverclass->m_pNext != nullptr; serverclass = serverclass->m_pNext)
	//	NWR_AddSendTable(serverclass->m_pTable);

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