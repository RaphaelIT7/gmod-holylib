#include "LuaInterface.h"
#include "detours.h"
#include "module.h"
#include "lua.h"
#include "recipientfilter.h"
#include "packed_entity.h"
#include "dt.h"

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

	Due to us using the latest version of CSendTablePrecalc this module only works on the dev & 64x branch of GMod
	Since this request was implemented on these: https://github.com/Facepunch/garrysmod-requests/issues/2981
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
	static constexpr int DPT_REALNUMSendPropTypes = DPT_NUMSendPropTypes+1;

	int m_nNewOffset = 0; // We inherit m_Offset already, this is to map the old offset to our new one!
	int m_nNewSize = 0; // Size in our new struct - in bytes! (if -1 then it's a bool!)
	int m_nBitOffset = 0; // For bool types since we pack them into a byte
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
			if (type == SendPropType::DPT_Int && pProp->m_nBits == 1)
			{
				pProps[HolyLibSendPropPrecalc::DPT_BOOL].AddToTail(pProp);
			} else {
				pProps[type].AddToTail(pProp);
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
					nBits = sizeof(float) * 8 * 3;
					break;
				case SendPropType::DPT_VectorXY:
					nBits = sizeof(float) * 8 * 2;
					break;
				case SendPropType::DPT_String: // SHIT! SendPropString needs to have this line added IN GMOD, else we can't know the size! ret.m_nBits = bufferLen * 8;
					nBits = (sizeof(char) * 8 * DT_MAX_STRING_BUFFERSIZE) + 8;
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

void SendProp_FillSnapshot( HolyLibCSendTablePrecalc *pTable, PackedEntity* pPack, const void* pEntity )
{
	int nCurrentByte = 0;
	for (int nDPTType = 0; nDPTType < HolyLibSendPropPrecalc::DPT_REALNUMSendPropTypes; ++nDPTType)
	{
		const HolyLibCSendTablePrecalc::SendPropStruct& pStruct = pTable->m_SendPropStruct[nDPTType];

		if (nDPTType ==  HolyLibSendPropPrecalc::DPT_BOOL)
		{
			FOR_EACH_VEC( pStruct.m_pProps, i )
			{
				const HolyLibSendPropPrecalc* pProp = pStruct.m_pProps[i];
				bool* pVar = (bool*)((char*)pEntity + pProp->GetOffset());
				*((uint8*)((char*)pPack->GetData() + nCurrentByte + pProp->m_nNewOffset)) |= (*pVar ? 1 : 0) << pProp->m_nBitOffset;
			}

			nCurrentByte += pStruct.m_nBytes;
		} else {
			FOR_EACH_VEC( pStruct.m_pProps, i )
			{
				const HolyLibSendPropPrecalc* pProp = pStruct.m_pProps[i];
				memcpy((char*)pPack->GetData() + nCurrentByte, (char*)pEntity + pProp->GetOffset(), pProp->m_nNewSize);
				nCurrentByte += pProp->m_nNewSize;
			}
		}
	}
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
}

/*
	Detouring
*/

void CNetworkingReplacementModule::InitDetour(bool bPreServer)
{
	if (bPreServer)
		return;

	// Detour 
}