#pragma once

#include "util.h"
#include "bitvec.h"

extern "C"
{
	#include "../luajit/src/lj_obj.h"
}

namespace GarrysMod::Lua
{
	class ILuaShared;
}

// Enables support for cdata to be used as userdata
// See the RawLua::CDataBridge to know how it works
#define LUA_CDATA_SUPPORT 1
namespace RawLua {
	struct CDataBridge
	{
		// Registers the given cdata type to be treated as the given metaID
		void RegisterType(unsigned char pMetaID, uint16_t cTypeID)
		{
			pRegisteredTypes.Set(cTypeID);
			pMetaIDToCType[pMetaID] = cTypeID; // Though currently unused.
		}

		// Registers the ctype that represents HolyLib userdata.
		void RegisterHolyLibUserData(int nTypeID)
		{
			nHolyLibUserDataTypeID = nTypeID;
			pRegisteredTypes.Set(nTypeID); // Mark this too as else HolyLib's userdata would be seen as cdata and it'll shit everything
		}

		// If a cdata type was never registered, then it will never be allowed to act as userdata
		FORCEINLINE bool IsRegistered(TValue* val)
		{
			return pRegisteredTypes.IsBitSet(cdataV(val)->ctypeid);
		}

		int GetHolyLibUserDataGCFuncRef()
		{
			return nHolyLibUserDataGCFuncReference;
		}

		void SetHolyLibUserDataGCFuncRef(int nRef)
		{
			nHolyLibUserDataGCFuncReference = nRef;
		}

		TValue* GetHolyLibUserData()
		{
			return &nHolyLibUserDataGC;
		}

	private:
		CBitVec<USHRT_MAX> pRegisteredTypes;
		std::unordered_map<unsigned char, uint16_t> pMetaIDToCType = {};
		uint32_t nHolyLibUserDataTypeID = 0; // cData TypeID of the HOLYLIB_UserData struct.
		TValue nHolyLibUserDataGC;
		int nHolyLibUserDataGCFuncReference = -1;
	};

	extern TValue* index2adr(lua_State *L, int idx);
	extern TValue* CopyTValue(lua_State* L, TValue* o);
	extern void DestroyTValue(TValue* o);
	extern void PushTValue(lua_State* L, TValue* o);
	extern void SetReadOnly(TValue* o, bool readOnly);
	extern void* GetUserDataOrFFIVar(lua_State* L, int idx, CDataBridge& cDataBridge, bool* bIsUserData);
	extern uint16_t GetCDataType(lua_State* L, int idx);

	// Expects it to only be used from jit.registerCreateHolyLibCDataFunc where on the stack 2 MUST be a table.
	extern uint32_t GetCTypeFromName(lua_State* L, const char* pName, CDataBridge* cDataBridge);

	// Allocates either a cData if there is a registered ctype and else it allocates userdata.
	// Returns a pointer to the userdata.
	extern void* AllocateCDataOrUserData(GarrysMod::Lua::ILuaInterface* pLua, int nMetaID, int nSize);
}

struct LuaUserData;
class CLuaInterfaceProxy;
namespace Lua
{
	extern void Init(GarrysMod::Lua::ILuaInterface* LUA);
	extern void Shutdown();
	extern void FinalShutdown();
	extern void ServerInit();

	/*
	   Hooks ALWAYS run on g_Lua.
	   Tries to push hook.Run and the given string.
	   Stack:
	   -2 = hook.Run(function)
	   -1 = hook name(string)

	   When you call CallFunctionProtected you need to use your number of arguments + 1 since the hook name is also an argument!
	 */
	extern bool PushHook(const char* pName, GarrysMod::Lua::ILuaInterface* pLua = g_Lua);
	extern void AddDetour();
	extern void SetManualShutdown();
	extern void ManualShutdown();
	extern GarrysMod::Lua::ILuaInterface* GetRealm(unsigned char);
	extern GarrysMod::Lua::ILuaShared* GetShared();

	extern GarrysMod::Lua::ILuaInterface* CreateInterface();
	extern void DestroyInterface(GarrysMod::Lua::ILuaInterface* LUA);

	// Each new metatable has this entry.
	struct LuaMetaEntry {
		unsigned char iType = UCHAR_MAX;
		unsigned char iLuaType = UCHAR_MAX;
		GCRef metatable; // Direct reference allowing for faster setting.

		LuaMetaEntry()
		{
			setgcrefnull(metatable);
		}
	};

	/*
		All lua types that exist.
		Use LUA_className as formatting.
	*/
	enum LuaTypes : unsigned char {
		IGModAudioChannel,
		bf_read,
		bf_write,
		CBaseClient,
		CHLTVClient,
		QAngle,
		Vector,
		EntityList,
		ConVar,
		IPhysicsCollisionSet,
		CPhysCollide,
		CPhysPolysoup,
		CPhysConvex,
		ICollisionQuery,
		IPhysicsObject,
		ILuaPhysicsEnvironment,
		IGameEvent,
		HttpResponse,
		HttpRequest,
		HttpServer,
		VoiceData,
		VoiceStream,
		CNetChan,
		INetworkStringTable,
		CVProfNode,
		VProfCounter,
		LuaInterface,
		// WavAudioFile,

		_HOLYLIB_CORE_TEST = UCHAR_MAX-1,
		TOTAL_TYPES = UCHAR_MAX,
	};

	class ModuleData {
	public:
		virtual ~ModuleData() = default; // Virtual deconstructor if anyone has their own deleting process
	};

	namespace Internal {
		static constexpr int pMaxEntries = 64;
	}

	// A structure in which modules can store data specific to a ILuaInterface.
	// This will be required when we work with multiple ILuaInterface's
	struct StateData
	{
		void* pOtherData[4]; // If any other plugin wants to use this, they can.
		// ID 0 is used by HolyLib core!!!
		// It uses the assigned module IDs
		Lua::ModuleData* pModuelData[Lua::Internal::pMaxEntries] = { NULL };
		LuaMetaEntry pLuaTypes[LuaTypes::TOTAL_TYPES];
		std::unordered_map<void*, LuaUserData*> pPushedUserData; // Would love to get rid of this
		GarrysMod::Lua::ILuaInterface* pLua = NULL;
		CLuaInterfaceProxy* pProxy;
		GCRef nErrorFunc;
#if LUA_CDATA_SUPPORT
		RawLua::CDataBridge pBridge;
#endif

		StateData()
		{
			setgcrefnull(nErrorFunc);

			for (int i=0; i<LuaTypes::TOTAL_TYPES; ++i)
				pLuaTypes[i].iLuaType = i;
		}

		~StateData();

		// When called we expect the error function to be pushed onto the stack. We'll pop it.
		inline void SetErrorFunc()
		{
			lua_State* L = pLua->GetState();
			TValue* pVal = RawLua::index2adr(L, -1);
			if (!tvisfunc(pVal))
			{
				Warning(PROJECT_NAME " - SetErrorFunc: -1 stack pos is NOT a function?!? What the heck!\n");
				pLua->Pop(1);
				return;
			}

			setgcref(nErrorFunc, obj2gco(tabV(pVal)));
			pLua->Pop(1); // it's stored in the registry so everything will be fine :3
		}

		inline void RegisterMetaTable(LuaTypes type, int metaID)
		{
			pLuaTypes[type].iType = (unsigned char)metaID;
			setgcrefnull(pLuaTypes[type].metatable);
			Msg(PROJECT_NAME " - RegisterMetaTable: Registered MetaTable: %i - %i\n", (int)type, metaID);

			if (!pLua) // Should never be null but just in case
				return;

			if (!pLua->PushMetaTable(metaID))
			{
				Warning(PROJECT_NAME " - RegisterMetaTable: Failed to push metatable %i!\n", metaID);
				return;
			}

			lua_State* L = pLua->GetState();
			TValue* pVal = RawLua::index2adr(L, -1);
			if (!tvistab(pVal))
			{
				Warning(PROJECT_NAME " - RegisterMetaTable: MetaTable is NOT a table?!? What the heck!\n");
				pLua->Pop(1);
				return;
			}

			setgcref(pLuaTypes[type].metatable, obj2gco(tabV(pVal)));
			pLua->Pop(1); // Since metatables are stored in the registry, our refernce should never change and always be valid :3
		}

		inline unsigned char GetMetaTable(LuaTypes type)
		{
			return pLuaTypes[type].iType;
		}

		inline const LuaMetaEntry& GetMetaEntry(LuaTypes type)
		{
			return pLuaTypes[type];
		}

		inline LuaTypes FindMetaTable(unsigned char type)
		{
			for (int i=0; i<LuaTypes::TOTAL_TYPES; ++i)
			{
				if (pLuaTypes[i].iType == type)
					return (LuaTypes)i;
			}

			return LuaTypes::TOTAL_TYPES;
		}

		/*
		 * Returns the module data by the given module id.
		 */
		inline void* GetModuleData(int moduleID)
		{
			if (moduleID >= Lua::Internal::pMaxEntries || moduleID < 0) // out of bounds
				return NULL;

			return (void*)pModuelData[moduleID];
		}

		/*
		 * Sets the given module data by the given module id.
		 */
		inline void SetModuleData(int moduleID, Lua::ModuleData* moduleData)
		{
			if (moduleID >= Lua::Internal::pMaxEntries || moduleID < 0) // out of bounds
			{
				Warning(PROJECT_NAME ": Tried to set module id for a module out of range! (%i)\n", moduleID);
				return;
			}

			pModuelData[moduleID] = moduleData;
		}

		inline std::unordered_map<void*, LuaUserData*>& GetPushedUserData()
		{
			return pPushedUserData;
		}

		// No error handling
		inline bool FastPCall(int nArgs, int nRets, bool bShowError)
		{
			lua_State* L = pLua->GetState();
			int nRet = Util::func_lua_pcall(L, nArgs, nRets, 0);
			if (nRet > 0)
			{
				// We use the ILuaInterface here since on errors we really don't expect speed.
				if (bShowError)
				{
					const char* pError = pLua->GetString(-1);
					pLua->Pop(1);
					pLua->ErrorFromLua(pError);
				} else {
					pLua->Pop(1);
				}

				return false;
			}

			return true;
		}

#if LUA_CDATA_SUPPORT
		inline RawLua::CDataBridge& GetCDataBridge()
		{
			return pBridge;
		}
#endif
	};

	/*
	 * Where do we store our StateData?
	 * In the ILuaInterface itself.
	 * We abuse the GetPathID var as it's a char[32] but it'll never actually fully use it.
	 * Why? Because I didn't want to use yet another unordered_map for this, also this should be faster.
	 */
	// constexpr int nPathIDOffset = 196 + 24;
	FORCEINLINE Lua::StateData* GetLuaData(GarrysMod::Lua::ILuaInterface* LUA)
	{
		return *reinterpret_cast<Lua::StateData**>((char*)LUA->GetPathID() + 24); // ((char*)LUA + nPathIDOffset);
	}
	extern void CreateLuaData(GarrysMod::Lua::ILuaInterface* LUA, bool bNullOut = false);
	extern void RemoveLuaData(GarrysMod::Lua::ILuaInterface* LUA);
	extern const std::unordered_set<Lua::StateData*>& GetAllLuaData();

	// In a single call checks the type and returns the userdata saving some work.
	extern bool CheckHolyLibType(GarrysMod::Lua::ILuaInterface* LUA, int nStackPos, int nType, LuaUserData** pUserData);
	extern LuaUserData* GetHolyLibUserData(GarrysMod::Lua::ILuaInterface* LUA, int nStackPos);

	extern bool FindOnObjectsMetaTable(lua_State* L, int nStackPos, int nKeyPos);

	// GMod specific fast type check
	extern bool CheckGModType(GarrysMod::Lua::ILuaInterface* LUA, int nStackPos, int nType, void** pUserData);

	// ToDo
	// - EnterLockdown
	// - LeaveLockdown
	// What do they do?
	// Their used when All ILuaInterfaces need to be locked in cases like a referenced userdata being deleted.
	// This is because, a referenced userdata can be shared across threads.
	// This sounds like a dumb idea, because it is.
	// it would be painful to implement differently.
}

// Creates a function Get[funcName]LuaData and returns the stored module data from the given module.
#define LUA_GetModuleDataWithID(className, funcName, id) \
static inline className* Get##funcName##LuaData(GarrysMod::Lua::ILuaInterface* pLua) \
{ \
	if (!pLua) { \
		return NULL; \
	} \
\
	return (className*)Lua::GetLuaData(pLua)->GetModuleData(id); \
}

#define LUA_GetModuleData(className, moduleName, funcName) LUA_GetModuleDataWithID(className, funcName, moduleName.m_pID)

/*
	==============================================
	= Now follows the huge stuff for LuaUserData =
	==============================================
*/

// Lua's GCudata struct, same on 2.1 & 2.0 so this should work everywere.
// We use our own version of it because we can save memory by doing so, gmod could do the same yet they chose to not to??? idk.
typedef struct GCudata_holylib { // We cannot change layout/sizes.
	GCHeader; // GCHeader
	uint8_t udtype;	/* Userdata type. */
	uint8_t flags;	/* Unused normally - we use it to store flags */
	GCRef usertable;	/* Should be at same offset in GCfunc. Accessable using setfenv/getfenv though I don't think anyone knows xd */
	MSize len;		/* Size of payload. */
	GCRef metatable;	/* Must be at same offset in GCtab. */
	void* data;
} GCudata_holylib;
constexpr int GCudata_holylib_dataoffset = sizeof(GCudata_holylib) - sizeof(void*);

enum udataFlags // we use bit flags so only a total of 8 are allowed.v
{
	UDATA_EXPLICIT_DELETE = 1 << 0,
	UDATA_NO_GC = 1 << 1, // Causes additional flags to be set onto "marked" GCHeader var
	UDATA_NO_USERTABLE = 1 << 2,
	UDATA_INLINED_DATA = 1 << 3,
};

/*
	ToDo: Implement a proper class like gmod has with CLuaCLass/CLuaLibrary & use thoes instead for everything.
*/

/*
 * How to properly push & clean up LuaUserData.
 *
 * First, use the proper function for the userdata you want to push.
 *
 * Push_LuaClass - Use this macro if you want the Lua GC to free the userdata.
 * PushReferenced_LuaClass - Use this macro if you want to manually delete the userdata, it will hold a reference of itself stopping the GC.
 *
 * NOTE: If you use PushReferenced_LuaClass you should ALWAYS call DeleteAll_[YourClass] for your userdata
 *	   as else it COULD persist across lua states which is VERY BAD as the references will ALL be INVALID.
 */

// This WILL slow down userData creation & deletion so we disable this in release builds.
#if HOLYLIB_BUILD_RELEASE
#define HOLYLIB_UTIL_DEBUG_LUAUSERDATA 0
#else
// We no longer check for leaks since with our changes to use the GCudata directly we no longer have to worry about such things.
#define HOLYLIB_UTIL_DEBUG_LUAUSERDATA 0 // 1 = leak checks, 2 = debug prints
#endif
#define HOLYLIB_UTIL_DEBUG_BASEUSERDATA 0

/*
 * Base UserData struct that is used by LuaUserData.
 * Main purpose is for it to handle the stored pData & implement a reference counter.
 * It also maskes sharing data between threads easier.
 *
 * NOTE:
 * Memory usage is utter garbage.
 * For UserData which stores a few bytes (often 4) we setup UserData that has a average size of 60 bytes...
 * 
 * Update:
 * Memory usage improved noticably
 * Also we don't even need this class
 */

// We had previously kept stuff in a global unordered_map though we don't even need it at all
#define HOLYLIB_UTIL_GLOBALUSERDATA 0
#define HOLYLIB_UTIL_BASEUSERDATA 0

#if HOLYLIB_UTIL_BASEUSERDATA
struct LuaUserData;
struct BaseUserData;
#if HOLYLIB_UTIL_GLOBALUSERDATA
extern std::shared_mutex g_UserDataMutex; // Needed to keep g_pGlobalLuaUserData thread safe
extern std::unordered_map<void*, BaseUserData*> g_pGlobalLuaUserData; // A set containing all BaseUserData.
#endif
struct BaseUserData {
	BaseUserData(void* data)
	{
		m_pData = data;
#if HOLYLIB_UTIL_GLOBALUSERDATA
		std::unique_lock lock(g_UserDataMutex);
		g_pGlobalLuaUserData[m_pData] = this; // Link it
#endif
	}

	~BaseUserData()
	{
#if HOLYLIB_UTIL_GLOBALUSERDATA
		if (m_pData != NULL)
		{
			std::shared_lock lock(g_UserDataMutex);
			auto it = g_pGlobalLuaUserData.find(m_pData);
			if (it != g_pGlobalLuaUserData.end())
				g_pGlobalLuaUserData.erase(it);
		}
#endif

		m_pData = NULL;
	}

	inline void* GetData(GarrysMod::Lua::ILuaInterface* pLua)
	{
		//if (m_bLocked && pLua != m_pOwningLua)
		//	return NULL;

		return m_pData;
	}

	/*inline void SetData(void* data)
	{
		if (pData != NULL) // This UserData was reassigned?!? Shouldn't happen normally... Anyways.
		{
			auto it = g_pGlobalLuaUserData.find(pData);
			if (it != g_pGlobalLuaUserData.end())
				g_pGlobalLuaUserData.erase(it);
		}

		pData = data;
		g_pGlobalLuaUserData[pData] = this; // Link it
	}*/

	/*
	 * Locks/Unlocks the UserData making GetData only return a valid result for the owning ILuaInterface.
	 */
	inline void SetLocked(bool bLocked)
	{
		// m_bLocked = bLocked;
	}

	inline bool Release(LuaUserData* pLuaUserData)
	{
#if HOLYLIB_UTIL_DEBUG_BASEUSERDATA
		Msg("holylib - util: Userdata %p(%p) tried to release %i (%p)\n", this, m_pData, m_iReferenceCount, pLuaUserData);
#endif

#if HOLYLIB_UTIL_DEBUG_BASEUSERDATA
		Msg("holylib - util: Userdata %p(%p) got released %i (%p)\n", this, m_pData, m_iReferenceCount, pLuaUserData);
#endif

		if (m_iReferenceCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
#if HOLYLIB_UTIL_DEBUG_BASEUSERDATA
		Msg("holylib - util: Userdata %p(%p) got deleted %i (%p)\n", this, m_pData, m_iReferenceCount, pLuaUserData);
#endif
			delete this;
			return true;
		}

		return false;
	}

	static inline BaseUserData* LuaAquire(LuaUserData* pLuaUserData, void* pData)
	{
		if (!pData)
			return NULL;

#if HOLYLIB_UTIL_GLOBALUSERDATA
		std::shared_lock lock(g_UserDataMutex);
		auto it = g_pGlobalLuaUserData.find(pData);
		if (it != g_pGlobalLuaUserData.end())
		{
			it->second->Aquire();
			return it->second;
		}
#endif

		BaseUserData* pUserData = new BaseUserData(pData);
		pUserData->Aquire(); // Increment counter
		return pUserData;
	}

	inline int GetReferenceCount()
	{
		return m_iReferenceCount.load(std::memory_order_relaxed);
	}

private:
	inline void Aquire()
	{
		m_iReferenceCount.fetch_add(1, std::memory_order_relaxed);
#if HOLYLIB_UTIL_DEBUG_BASEUSERDATA
		Msg("holylib - util: Userdata %p(%p) got aquired %i\n", this, m_pData, m_iReferenceCount);
#endif
	}

	void* m_pData = NULL;
	// GarrysMod::Lua::ILuaInterface* m_pOwningLua = NULL; // ToDo: We don't even set the m_pOwningLua right now.

	//bool m_bLocked = false;
	std::atomic<unsigned int> m_iReferenceCount = 0;
};
#endif

#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA
struct LuaUserData;
extern std::unordered_set<LuaUserData*> g_pLuaUserData; // A set containing all LuaUserData that actually hold a reference.
#endif
struct LuaUserData : GCudata_holylib { // No constructor/deconstructor since its managed by Lua!
	// Will only be called when you stack allocate it.
	LuaUserData()
	{
		setgcrefnull(nextgc);
		gct = 0x0;
		marked = 0x4; // mark black. We are stack allocated, we are never white, never grey
	}
	
	inline void Init(GarrysMod::Lua::ILuaInterface* LUA, const Lua::LuaMetaEntry& pMetaEntry, void* pData, bool bNoGC = false, bool bNoUserTable = false, bool bIsInline = false)
	{
		// Since Lua creates our userdata, we need to set all the fields ourself!
#if HOLYLIB_UTIL_BASEUSERDATA
		pBaseData = NULL;
#endif

		flags = 0;
		if (!bIsInline)
			data = pData;
		else
			flags |= UDATA_INLINED_DATA;

		if (bNoGC)
		{
			marked |= 0x20 | 0x40;
			flags |= UDATA_NO_GC;
		}

		udtype = pMetaEntry.iType;
		metatable = pMetaEntry.metatable;

		if (pMetaEntry.iType == UCHAR_MAX)
		{
			Warning(PROJECT_NAME ": Tried to push userdata of HolyLib Lua Type %i but it was never registered! (Is the module missing/disabled?)", pMetaEntry.iLuaType);
		}

		if (bNoUserTable)
		{
			flags |= UDATA_NO_USERTABLE;
			// setgcrefnull(usertable); // Verify: Do we need to always have a valid usertable? iirc the gc is missing a null check
		} else {
			ClearLuaTable(LUA, true);
		}

		/*global_StateGMOD *g = (global_StateGMOD*)G(LUA->GetState());
		setgcrefr(nextgc, mainthread(g)->nextgc);
		setgcref(mainthread(g)->nextgc, obj2gco(this));

		// We set the length to our difference for Lua to not shit itself...
		static constexpr int udataSize = sizeof(LuaUserData) - sizeof(GCudata);
		len = udataSize;*/

#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA
		g_pLuaUserData.insert(this);
#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA == 2
		Msg("holylib - util: LuaUserdata got initialized %p - %p - %i\n", this, LUA, pMetaEntry.iType);
#endif
#endif
	}

#if HOLYLIB_UTIL_BASEUSERDATA
	inline void* GetData(GarrysMod::Lua::ILuaInterface* pLua)
	{
		return pBaseData ? pBaseData->GetData(pLua) : NULL;
	}

	inline BaseUserData* GetInternalData()
	{
		return pBaseData;
	}
#else
	inline void* GetData()
	{
		if (flags & UDATA_INLINED_DATA)
			return (void*)((char*)this + GCudata_holylib_dataoffset);

		return data;
	}
#endif

	inline void SetData(void* pData)
	{
#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA == 2
		Msg("holylib - util: LuaUserdata got new data %p - %p\n", this, data);
#endif

#if HOLYLIB_UTIL_BASEUSERDATA
		if (pBaseData != NULL)
		{
			pBaseData->Release(this);
			pBaseData = NULL;
		}

		if (data)
		{
			pBaseData = BaseUserData::LuaAquire(this, data);
		}
#else
		if (flags & UDATA_INLINED_DATA)
		{
			Warning(PROJECT_NAME " - LuaUserData: Tried to call SetData when the data is inlined into the userdata!\n");
			return;
		}

		data = pData;
#endif
	}

	inline void PushLuaTable(GarrysMod::Lua::ILuaInterface* pLua)
	{
		lua_State* L = pLua->GetState();
		if (flags & UDATA_NO_USERTABLE)
		{
			setnilV(L->top++);
		} else {
			/*if (gcrefu(usertable) == 0)
			{
				if (Util::func_lj_tab_new)
				{
					setgcref(usertable, obj2gco(Util::func_lj_tab_new(L, 0, 0)));
				} else { // Slower on 64x though I won't deal with that shit for this improvement.
					pLua->CreateTable();
					setgcref(usertable, obj2gco(tabV(L->top-1)));
				}
			} else {*/
				settabV(L, L->top++, gco2tab(gcref(usertable)));
			//}
		}
	}

	inline void ClearLuaTable(GarrysMod::Lua::ILuaInterface* pLua, bool bFresh = false) // bFresh = if we got freshly created / are in a white state
	{
		if (flags & UDATA_NO_USERTABLE)
			return;

		lua_State* L = pLua->GetState();
		if (Util::func_lj_tab_new && (bFresh || Util::func_lj_gc_barrierf))
		{
			// We cannot free it since the GC would kill itself... We also SHOULDN'T since the gc handles it already, so NO touching >:(
			// Why does the GC kill itself? Because inside the GCHeader of the usertable, the next GC object is stored.
			// Normally when we would delete it, we would adjust it and properly handle this by updating the "nextgc" of the previous gc object
			// though it's not double linked / there is no "previousgc" variable so we do not know the next gc object.
			// And lj_tab_free also doesn't take care of this, it just frees the memory, so this caused crashes, memory corruption and infinite loops.
			// Util::func_lj_tab_free(G(L), gco2tab(gcref(usertable)));
			setgcref(usertable, obj2gco(Util::func_lj_tab_new(L, 0, 0)));
			
			if (!bFresh)
			{ // lj_gc_objbarrier unwrapped since we need to change the call to lj_gc_barrierf -> Util::func_lj_gc_barrierf
				if (((((GCobj*)(gcref(usertable))))->gch.marked & (0x01 | 0x02)) && ((((GCobj*)(this)))->gch.marked & 0x04))
							Util::func_lj_gc_barrierf(G(L), ((GCobj*)(this)), ((GCobj*)(gcref(usertable))));
			}
		} else {
			pLua->CreateTable();
			if (!bFresh)
			{
				if (Util::func_lj_gc_barrierf)
				{
					setgcref(usertable, obj2gco(tabV(L->top-1)));

					{ // lj_gc_objbarrier unwrapped since we need to change the call to lj_gc_barrierf -> Util::func_lj_gc_barrierf
						if (((((GCobj*)(gcref(usertable))))->gch.marked & (0x01 | 0x02)) && ((((GCobj*)(this)))->gch.marked & 0x04))
							Util::func_lj_gc_barrierf(G(L), ((GCobj*)(this)), ((GCobj*)(gcref(usertable))));
					}
					pLua->Pop(1);
				} else {
					Util::func_lua_setfenv(L, -2); // Internally has the gc barrier
				}
			} else {
				setgcref(usertable, obj2gco(tabV(L->top-1)));
				pLua->Pop(1);
			}
		}
	}

	inline void Push(GarrysMod::Lua::ILuaInterface* pLua)
	{
		lua_State* L = pLua->GetState();
		setudataV(L, L->top++, static_cast<GCudata*>(static_cast<void*>(this)));
#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA == 2
		Msg("holylib - util: LuaUserdata pushing %p\n", this);
#endif
	}

	inline bool Release(GarrysMod::Lua::ILuaInterface* pLua, bool bGCCall = false)
	{
#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA == 2
		Msg("holylib - util: LuaUserdata got released %p - %p (%i, Called by GC: %s)\n", this, data, len, bGCCall ? "true" : "false");
#endif

#if HOLYLIB_UTIL_BASEUSERDATA
		if (pBaseData != NULL)
		{
			bool bDelete = pBaseData->Release(this);
			pBaseData = NULL;
			delete this;
			return bDelete;
		}

		delete this;
		return false;
#else

#if HOLYLIB_UTIL_DEBUG_LUAUSERDATA
		g_pLuaUserData.erase(this);
#endif

#if HOLYLIB_UTIL_BASEUSERDATA
		if (pBaseData != NULL)
		{
			pBaseData->Release(this);
			pBaseData = NULL;
		}
#endif
		if (!(flags & UDATA_INLINED_DATA))
			data = NULL;

		if (flags & UDATA_NO_GC)
		{
			marked &= (uint8_t)~(0x20 | 0x40); // Unset FIXED GC flags if they were set. Else GC is not gonna like this
		}

		return true;
#endif
	}

	inline unsigned char GetType()
	{
		return udtype;
	}

	static void ForceGlobalRelease(void* pData);

	inline bool IsFlagExplicitDelete()
	{
		return (flags & UDATA_EXPLICIT_DELETE) != 0;
	}

	inline void SetFlagExplicitDelete()
	{
		flags |= UDATA_EXPLICIT_DELETE;
	}

	inline bool IsInlined()
	{
		return (flags & UDATA_INLINED_DATA) != 0;
	}

	inline void SetAsInlined()
	{
		flags |= UDATA_INLINED_DATA;
	}

	inline int GetFlags()
	{
		return flags;
	}

private:
#if HOLYLIB_UTIL_BASEUSERDATA
	BaseUserData* pBaseData = NULL;
#endif
};
static constexpr int udataSize = sizeof(LuaUserData) - sizeof(GCudata);

#define TO_LUA_TYPE( className ) Lua::className

// BUG: This LuaClass function and all others were made in mind to support a single Lua Instance. Now we got multiple.
#define MakeString( str1, str2, str3 ) ((std::string)str1).append(str2).append(str3)
#define Get_LuaClass( className, strName ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
inline LuaUserData* Get_##className##_Data(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	unsigned char luaType = Lua::GetLuaData(LUA)->GetMetaTable(TO_LUA_TYPE(className)); \
	LuaUserData* pVar = nullptr; \
	if (!Lua::CheckHolyLibType(LUA, iStackPos, luaType, &pVar)) \
	{ \
		if (bError) \
			LUA->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	if ((!pVar || !pVar->GetData()) && bError) \
		LUA->ThrowError(triedNull_##className.c_str()); \
\
	return pVar; \
} \
\
className* Get_##className(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	LuaUserData* pLuaData = Get_##className##_Data(LUA, iStackPos, bError); \
	if (!pLuaData) \
		return NULL; \
 \
	return (className*)pLuaData->GetData(); \
}

#define GMODPush_LuaClass( className, luaType ) \
void Push_##className(GarrysMod::Lua::ILuaInterface* LUA, className* var) \
{ \
	if (!var) \
	{ \
		LUA->PushNil(); \
		return; \
	} \
\
	LUA->PushUserType(var, luaType); \
}

#define GMODGet_LuaClass( className, luaType, strName, additional ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
className* Get_##className(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	className* pVar = nullptr; \
	if (!Lua::CheckGModType(LUA, iStackPos, luaType, (void**)&pVar)) \
	{ \
		if (bError) \
			LUA->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	if (!pVar && bError) \
		LUA->ThrowError(triedNull_##className.c_str()); \
\
	additional \
\
	return pVar; \
}

// Only used by CBaseClient & CHLTVClient as they both work with each other / a CHLTVClient can use all CBaseClient functions.
#define SpecialGet_LuaClass( className, className2, strName, isvalid ) \
static std::string invalidType_##className = MakeString("Tried to use something that wasn't a ", strName, "!"); \
static std::string triedNull_##className = MakeString("Tried to use a NULL ", strName, "!"); \
inline LuaUserData* Get_##className##_Data(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	Lua::StateData* LUADATA = Lua::GetLuaData(LUA); \
	unsigned char luaType = LUADATA->GetMetaTable(TO_LUA_TYPE(className)); \
	unsigned char luaType2 = LUADATA->GetMetaTable(TO_LUA_TYPE(className2)); \
\
	LuaUserData* pVar = Lua::GetHolyLibUserData(LUA, iStackPos); \
	if (!pVar) \
		if (bError) \
			LUA->ThrowError(triedNull_##className.c_str()); \
	\
	if (!pVar || pVar->GetType() != luaType && pVar->GetType() != luaType2) \
	{ \
		if (bError) \
			LUA->ThrowError(invalidType_##className.c_str()); \
\
		return NULL; \
	} \
\
	return pVar; \
} \
\
className* Get_##className(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError) \
{ \
	LuaUserData* pLuaData = Get_##className##_Data(LUA, iStackPos, bError); \
	if (!pLuaData) \
		return NULL; \
 \
	className* pVar = (className*)pLuaData->GetData(); \
	if (!pVar || !isvalid) \
	{ \
		if (bError) \
			LUA->ThrowError(triedNull_##className.c_str()); \
\
		return NULL; \
	} \
\
	return pVar; \
}

#define Push_LuaClass( className ) \
static std::string triedPushing_##className = MakeString("Tried to push a ", #className, " class to Lua when it was never registered! (enable the module that registers them!)"); \
LuaUserData* Push_##className(GarrysMod::Lua::ILuaInterface* LUA, className* var) \
{ \
	if (!var) \
	{ \
		LUA->PushNil(); \
		return NULL; \
	} \
\
	const Lua::LuaMetaEntry& pMeta = Lua::GetLuaData(LUA)->GetMetaEntry(TO_LUA_TYPE(className)); \
	if (pMeta.iType == UCHAR_MAX) \
		LUA->ThrowError(triedPushing_##className.c_str()); \
	LuaUserData* userData = (LuaUserData*)((char*)RawLua::AllocateCDataOrUserData(LUA, pMeta.iType, udataSize) - sizeof(GCudata)); \
	userData->Init(LUA, pMeta, var); \
	return userData; \
} \
LuaUserData* PushInlined_##className(GarrysMod::Lua::ILuaInterface* LUA) \
{ \
	const Lua::LuaMetaEntry& pMeta = Lua::GetLuaData(LUA)->GetMetaEntry(TO_LUA_TYPE(className)); \
	if (pMeta.iType == UCHAR_MAX) \
		LUA->ThrowError(triedPushing_##className.c_str()); \
	LuaUserData* userData = (LuaUserData*)((char*)RawLua::AllocateCDataOrUserData(LUA, pMeta.iType, udataSize + sizeof(className) - sizeof(void*)) - sizeof(GCudata)); \
	userData->Init(LUA, pMeta, NULL, false, false, true); \
	return userData; \
}

/*
 * This one is special
 * the GC WONT free the LuaClass meaning this "could" (and did in the past) cause a memory/reference leak
 * The data thats passed won't be freed by lua.
 */
#pragma warning(disable:4505) // Why would we need a warning if a function got removed because its unused.
#define PushReferenced_LuaClass( className ) \
static std::string triedPushing_##className = MakeString("Tried to push a ", #className, " class to Lua when it was never registered! (enable the module that registers them!)"); \
void Push_##className(GarrysMod::Lua::ILuaInterface* LUA, className* var) \
{ \
	if (!var) \
	{ \
		LUA->PushNil(); \
		return; \
	} \
\
	auto& pushedUserData = Lua::GetLuaData(LUA)->GetPushedUserData(); \
	auto it = pushedUserData.find(var); \
	if (it != pushedUserData.end()) \
	{ \
		it->second->Push(LUA); \
	} else { \
		const Lua::LuaMetaEntry& pMeta = Lua::GetLuaData(LUA)->GetMetaEntry(TO_LUA_TYPE(className)); \
		if (pMeta.iType == UCHAR_MAX) \
			LUA->ThrowError(triedPushing_##className.c_str()); \
		LuaUserData* userData = (LuaUserData*)((char*)LUA->NewUserdata(udataSize) - sizeof(GCudata)); \
		userData->Init(LUA, pMeta, var, true); \
		pushedUserData[var] = userData; \
	} \
} \
\
[[maybe_unused]] static void Delete_##className(GarrysMod::Lua::ILuaInterface* LUA, className* var) \
{ \
	auto& pushedUserData = Lua::GetLuaData(LUA)->GetPushedUserData(); \
	auto it = pushedUserData.find(var); \
	if (it != pushedUserData.end()) \
	{ \
		it->second->Release(LUA); \
		pushedUserData.erase(it); \
	} \
} \
\
[[maybe_unused]] static void DeleteAll_##className(GarrysMod::Lua::ILuaInterface* LUA) \
{ \
	Lua::StateData* LUADATA = Lua::GetLuaData(LUA); \
	unsigned char luaType = LUADATA->GetMetaTable(TO_LUA_TYPE(className)); \
	auto& pushedUserData = LUADATA->GetPushedUserData(); \
	for (auto it = pushedUserData.begin(); it != pushedUserData.end(); ) \
	{ \
		if (it->second->GetType() == luaType) \
		{ \
			it->second->Release(LUA); \
			it = pushedUserData.erase(it); \
		} else { \
			it++; \
		} \
	} \
} \
\
[[maybe_unused]] static void DeleteGlobal_##className(className* var) \
{ \
	LuaUserData::ForceGlobalRelease(var); \
}

// A default index function for userData,
// which handles everything like finding the functions in the metatable
// or getting vars on the object like: print(userData.value)
#define Default__index(className) \
static int className ## __index( lua_State* L ) \
{ \
	if (Lua::FindOnObjectsMetaTable(L, 1, 2)) \
		return 1; \
\
	GarrysMod::Lua::ILuaInterface* LUA = ((lua_GmodState*)L)->luabase; \
	LUA->SetState(L); \
	Get_##className##_Data(LUA, 1, true)->PushLuaTable(LUA); \
	if (!LUA->FindObjectOnTable(-1, 2)) \
		LUA->PushNil(); \
\
	LUA->Remove(-2); \
\
	return 1; \
}

// A default newindex function for userData,
// handles setting vars on the object like: userData.value = true
#define Default__newindex(className) \
LUA_FUNCTION_STATIC(className ## __newindex) \
{ \
	Get_##className##_Data(LUA, 1, true)->PushLuaTable(LUA); \
	LUA->Push(2); \
	LUA->Push(3); \
	LUA->RawSet(-3); \
	LUA->Pop(1); \
\
	return 0; \
}

// A default gc function for userData,
// handles garbage collection, inside the func argument you can use pStoredData
// Use the pStoredData variable as pData->GetData() will return NULL. Just see how it's done inside the bf_read gc definition.
// NOTE: You need to manually delete the data inside the callback function -> delete pStoredData
// WARNING: DO NOT USE pData as it will be invalid because of the Release call!
#define Default__gc(className, func) \
LUA_FUNCTION_STATIC(className ## __gc) \
{ \
	LuaUserData* pData = Get_##className##_Data(LUA, 1, false); \
	if (pData) \
	{ \
		int pFlags = pData->GetFlags(); \
		bool bFlagExplicitDelete = pData->IsFlagExplicitDelete(); \
		bool bIsInlined = pData->IsInlined(); \
		void* pStoredData = pData->GetData(); \
		if (pData->Release(LUA, true)) \
		{ \
			pData = NULL; /*Don't let any idiot(that's me) use it*/ \
			func \
		} \
	} \
 \
	return 0; \
} \

// Default function for GetTable. Simple.
#define Default__GetTable(className) \
LUA_FUNCTION_STATIC(className ## _GetTable) \
{ \
	Get_##className##_Data(LUA, 1, true)->PushLuaTable(LUA); \
	return 1; \
}

/*
 * Shouldn't we have a Default__eq?
 * No.
 * Because if we push the same object, we will ALWAYS use PushReferenced_LuaClass
 * and since it's the same userdata thats reference pushed, we won't need a __eq
 */

#define MISSING_MODULE_ERROR(lua, modulename) lua->ThrowError(#modulename " module is missing in this holylib build!");

/*
	Push functions for Gmod Types
*/ 
Vector* Get_Vector(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError = true);
QAngle* Get_QAngle(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError = true);

class IRecipientFilter;
extern IRecipientFilter* Get_IRecipientFilter(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

class ConVar;
extern ConVar* Get_ConVar(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);


/*
	Push functions for HolyLib types.
*/

#if MODULE_EXISTS_BITBUF
class bf_read;
/*
 * It will be deleted by Lua GC.
 * ensure that either you set it to NULL afterwards or push a Copy if you don't own the original.
 */
extern LuaUserData* Push_bf_read(GarrysMod::Lua::ILuaInterface* LUA, bf_read* tbl, bool bDeleteUs); // if bDeleteUs is false, the bf_read won't be deleted when __gc is called.
extern bf_read* Get_bf_read(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);

class bf_write;
/*
 * It will be deleted by Lua GC.
 * ensure that either you set it to NULL afterwards or push a Copy if you don't own the original.
 */
extern LuaUserData* Push_bf_write(GarrysMod::Lua::ILuaInterface* LUA, bf_write* tbl, bool bDeleteUs);
extern bf_write* Get_bf_write(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);
#endif

#if MODULE_EXISTS_GAMEEVENT
class IGameEvent;
extern IGameEvent* Get_IGameEvent(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);
#endif

#if MODULE_EXISTS_ENTITYLIST
struct EntityList // entitylist module.
{
	EntityList();
	~EntityList();

	void Clear();
	inline bool IsValidReference(GCudata* iReference) { return iReference != nullptr; };
	void CreateReference(CBaseEntity* pEntity, bool bNoPop = false);
	void FreeEntity(CBaseEntity* pEntity);

	inline void PushReference(CBaseEntity* pEntity, GCudata* iReference)
	{
		if (!IsValidReference(iReference)) {
			CreateReference(pEntity, true);
		} else {
			lua_State* L = m_pLua->GetState();
			setudataV(L, L->top++, iReference);
		}
	}

	inline void AddEntity(CBaseEntity* pEntity, bool bCreateReference = false)
	{
		m_pEntities.push_back(pEntity);
		if (bCreateReference)
			CreateReference(pEntity);
		else
			m_pEntReferences[pEntity] = nullptr;
	}

	inline const std::unordered_map<CBaseEntity*, GCudata*>& GetReferences()
	{
		return m_pEntReferences;
	}

	inline const std::vector<CBaseEntity*>& GetEntities()
	{
		return m_pEntities;
	}

	inline void Invalidate()
	{
		Clear();
		m_pLua = NULL;
	}

	inline void SetLua(GarrysMod::Lua::ILuaInterface* pLua)
	{
		m_pLua = pLua;
	}

private:
	// NOTE: The Entity will always be valid but the reference can be -1!
	std::unordered_map<CBaseEntity*, GCudata*> m_pEntReferences;
	std::vector<CBaseEntity*> m_pEntities;
	GarrysMod::Lua::ILuaInterface* m_pLua = NULL;
};

extern EntityList& GetGlobalEntityList(GarrysMod::Lua::ILuaInterface* pLua);

extern bool Is_EntityList(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos);
extern EntityList* Get_EntityList(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);
#endif

#if MODULE_EXISTS_GAMESERVER
class CBaseClient;
extern void Push_CBaseClient(GarrysMod::Lua::ILuaInterface* LUA, CBaseClient* tbl);
extern CBaseClient* Get_CBaseClient(GarrysMod::Lua::ILuaInterface* LUA, int iStackPos, bool bError);
#endif

extern void Push_QAngle(GarrysMod::Lua::ILuaInterface* LUA, QAngle* var);
extern void Push_Vector(GarrysMod::Lua::ILuaInterface* LUA, Vector* var);