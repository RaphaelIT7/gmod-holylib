#include "module.h"
#include "LuaInterface.h"
#include "lua.h"

#if PLATFORM_64BITS
#include "../gmod-luajit/luajit21/lj_jit.h"
#include "../gmod-luajit/luajit21/lj_dispatch.h"
#include "../gmod-luajit/luajit21/lj_tab.h"
#include "../gmod-luajit/luajit21/lj_gdbjit.h"
#else
#include "../gmod-luajit/luajit20/lj_jit.h"
#include "../gmod-luajit/luajit20/lj_dispatch.h"
#include "../gmod-luajit/luajit20/lj_tab.h"
#include "../gmod-luajit/luajit20/lj_gdbjit.h"
#endif

class CLuaGCModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void Shutdown() override;
	const char* Name() override { return "luagc"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

static CLuaGCModule g_pLuaGCModule;
IModule* pLuaGCModule = &g_pLuaGCModule;

/*
	We generate a luagc_jit file which will contain all luagc functions BUT build with the headers of OUR LuaJIT build.

HOLYLIB_SETUP_FILE=sourcesdk/luagc_jit.cpp
HOLYLIB_SETUP_FILE_DEPENDING_MODULE=luajit
HOLYLIB_SETUP_FILE_REPLACE_PER_LINE=LUA_FUNCTION_STATIC(==>LUA_FUNCTION(luajit_
HOLYLIB_SETUP_FILE_CONTENTS_BEGIN
// THIS IS A GENERATED FILE!!! Modify this in modules/luagc.cpp
#define DISABLE_GMODJIT
#include "LuaInterface.h"
#include "lua.h"

extern "C"
{
	#include "../luajit/src/lj_jit.h"
	#include "../luajit/src/lj_dispatch.h"
	#include "../luajit/src/lj_tab.h"
	#include "../luajit/src/lj_gdbjit.h"
	#include "../luajit/src/lj_str.h"
}

// sizestring - previous macro in lj_obj but was removed in newer JIT versions
#define sizestring(str) lj_str_size(str->len)

HOLYLIB_SETUP_FILE_SKIPNEXTLINE // Skip the comment end below
*/

LUA_FUNCTION_STATIC(luagc_GetGCCount)
{
	global_State* pGState = G(LUA->GetState());
	if (!pGState)
	{
		LUA->PushNumber(0);
		return 1;
	}

	// Allows you to pass this function an GC object
	// This causes us to stop when we reach it
	GCobj* pTargetObject = nullptr;
	TValue* pVal = Lua::index2adr(LUA->GetState(), 1);
	if (tvisgcv(pVal))
		pTargetObject = gcV(pVal);

	int nCount = 0;
	GCobj* pObj = gcref(pGState->gc.root);
	while (pObj && pObj != pTargetObject)
	{
		++nCount;
		pObj = gcref(pObj->gch.nextgc);
	}

	LUA->PushNumber(nCount);
	return 1;
}

// Simple class to help us avoid recursion or checking already checked paths
class TraverseInfo
{
public:
	void AddNext(GCobj* pObj)
	{
		if (m_bLocked)
			return;

		auto it = m_Visited.find(pObj);
		if (it != m_Visited.end())
			return;

		m_Visited.insert(pObj);
		m_Queued.push_back(pObj);
	}

	GCobj* GetNext()
	{
		if (m_Queued.empty())
			return nullptr;

		GCobj* pObj = m_Queued.front();
		m_Queued.pop_front();
		return pObj;
	}
	
	inline void Lock()
	{
		m_bLocked = true;
	}

	unordered_set<GCobj*> m_Visited;
	std::deque<GCobj*> m_Queued;
	bool m_bLocked = false;
};

static bool LuaGC_ReferenceCheck(GCobj* pTargetObj, GCobj* pObj, lua_State* L)
{
	if (!pObj)
		return false;

	switch(pObj->gch.gct)
	{
	case ~LJ_TUPVAL:
		{
			GCupval* pVal = gco2uv(pObj);

			TValue* pTV = uvval(pVal);
			if (pTV && tvisgcv(pTV))
				if (gcV(pTV) == pTargetObj)
					return true;
		}
		break;
	case ~LJ_TUDATA:
		{
			GCudata* pVal = gco2ud(pObj);
			if (gcref(pVal->env) == pTargetObj)
				return true;

			if (gcref(pVal->metatable) == pTargetObj)
				return true;
		}
		break;
	case ~LJ_TTAB:
		{
			GCtab* pVal = gco2tab(pObj);
			if (gcref(pVal->metatable) == pTargetObj)
				return true;

			MSize i, asize = pVal->asize;
			for (i = 0; i < asize; i++)
			{
				TValue* pTV = arrayslot(pVal, i);
				if (tvisgcv(pTV))
					if (gcV(pTV) == pTargetObj)
						return true;
			}

			if (pVal->hmask > 0)
			{
				Node *node = noderef(pVal->node);
				MSize hmask = pVal->hmask;
				for (i = 0; i <= hmask; i++)
				{
					Node *n = &node[i];
					if (!tvisnil(&n->val))
					{
						if (tvisgcv(&n->key))
							if (gcV(&n->key) == pTargetObj)
								return true;

						if (tvisgcv(&n->val))
							if (gcV(&n->val) == pTargetObj)
								return true;
					}
				}
			}
		}
		break;
	case ~LJ_TFUNC:
		{
			GCfunc* pVal = gco2func(pObj);
			if (gcref(pVal->c.env) == pTargetObj)
				return true;

			if (isluafunc(pVal))
			{
				if (obj2gco(funcproto(pVal)) == pTargetObj)
					return true;

				for (uint32_t i = 0; i < pVal->l.nupvalues; i++)
					if (obj2gco(&gcref(pVal->l.uvptr[i])->uv) == pTargetObj)
						return true;

			} else {
				for (uint32_t i = 0; i < pVal->c.nupvalues; i++)
				{
					TValue* pTV = &pVal->c.upvalue[i];
					if (tvisgcv(pTV))
						if (gcV(pTV) == pTargetObj)
							return true;
				}
			}
		}
		break;
	case ~LJ_TPROTO:
		{
			GCproto* pVal = gco2pt(pObj);
			if (obj2gco(proto_chunkname(pVal)) == pTargetObj)
				return true;

			for (ptrdiff_t i = -(ptrdiff_t)pVal->sizekgc; i < 0; i++)
				if (proto_kgc(pVal, i) == pTargetObj)
					return true;

			global_State* g = G(L);
			if (pVal->trace)
				if (obj2gco(traceref(G2J(g), pVal->trace)) == pTargetObj)
					return true;
		}
		break;
	case ~LJ_TTRACE:
		{
			GCtrace* pVal = gco2trace(pObj);
			if (gcref(pVal->startpt) == pTargetObj)
				return true;

			IRRef ref;
			if (pVal->traceno == 0)
				break;
			
			for (ref = pVal->nk; ref < REF_TRUE; ref++) {
				IRIns *ir = &pVal->ir[ref];
				if (ir->o == IR_KGC)
					if (ir_kgc(ir) == pTargetObj)
						return true;

				if (irt_is64(ir->t) && ir->o != IR_KNULL)
					ref++;
			}

			global_State* g = G(L);
			if (pVal->nextside)
				if (obj2gco(traceref(G2J(g), pVal->nextside)) == pTargetObj)
					return true;

			if (pVal->link)
				if (obj2gco(traceref(G2J(g), pVal->link)) == pTargetObj)
					return true;

			if (pVal->nextroot)
				if (obj2gco(traceref(G2J(g), pVal->nextroot)) == pTargetObj)
					return true;
		}
		break;
	case ~LJ_TTHREAD:
		{
			lua_State* pVal = gco2th(pObj);
			if (gcref(pVal->env) == pTargetObj)
				return true;

			GCupval* pUpVal = gcref(pVal->openupval) ? gco2uv(gcref(pVal->openupval)) : NULL;
			while (pUpVal)
			{
				TValue* pUpValTV = uvval(pUpVal);
				if (pUpValTV && tvisgcv(pUpValTV))
				{
					if (obj2gco(gcV(pUpValTV)) == pTargetObj)
						return true;
				}

				pUpVal = uvnext(pUpVal);
			}

			TValue* pBase = Lua::LuaBase(pVal);
			int nTop = (int)(Lua::LuaTop(pVal) - pBase);
			for (int i=0; i<nTop; ++i)
			{
				if (tvisgcv(pBase))
					if (gcval(pBase) == pTargetObj)
						return true;

				pBase++;
			}
		}
		break;
	default:
		break;
	}

	return false;
}

LUA_FUNCTION_STATIC(luagc_GetReferences)
{
	LUA->PreCreateTable(0, 0);
	lua_State* L = LUA->GetState();
	global_State* pGState = G(L);
	if (!pGState)
		return 1;

	TValue* pVal = Lua::index2adr(L, 1);
	if (!tvisgcv(pVal))
		return 1;

	GCobj* pTargetObject = gcV(pVal);
	int nCount = 0;
	GCobj* pObj = gcref(pGState->gc.root);
	while (pObj)
	{
		if (LuaGC_ReferenceCheck(pTargetObject, pObj, L))
		{
			LUA->PushNil();
			setgcV(L, Lua::LuaTop(L)-1, pObj, ~pObj->gch.gct);
			Util::RawSetI(LUA, -2, ++nCount);
		}

		pObj = gcref(pObj->gch.nextgc);
	}

	return 1;
}

static void LuaGC_References(GCobj* pObj, TraverseInfo& info, lua_State* L, GarrysMod::Lua::ILuaInterface* LUA)
{
	if (!pObj)
		return;

	switch(pObj->gch.gct)
	{
	case ~LJ_TUPVAL:
		{
			GCupval* pVal = gco2uv(pObj);

			TValue* pTV = uvval(pVal);
			if (pTV && tvisgcv(pTV))
				info.AddNext(gcV(pTV));
		}
		break;
	case ~LJ_TUDATA:
		{
			GCudata* pVal = gco2ud(pObj);
			info.AddNext(gcref(pVal->env));
			info.AddNext(gcref(pVal->metatable));
		}
		break;
	case ~LJ_TTAB:
		{
			GCtab* pVal = gco2tab(pObj);
			info.AddNext(gcref(pVal->metatable));
			
			MSize i, asize = pVal->asize;
			for (i = 0; i < asize; i++)
			{
				TValue* pTV = arrayslot(pVal, i);
				if (tvisgcv(pTV))
					info.AddNext(gcV(pTV));
			}

			if (pVal->hmask > 0)
			{
				Node *node = noderef(pVal->node);
				MSize hmask = pVal->hmask;
				for (i = 0; i <= hmask; i++)
				{
					Node *n = &node[i];
					if (!tvisnil(&n->val))
					{
						if (tvisgcv(&n->key))
							info.AddNext(gcV(&n->key));

						if (tvisgcv(&n->val))
							info.AddNext(gcV(&n->val));
					}
				}
			}
		}
		break;
	case ~LJ_TFUNC:
		{
			GCfunc* pVal = gco2func(pObj);
			info.AddNext(gcref(pVal->c.env));
			
			if (isluafunc(pVal))
			{
				info.AddNext(obj2gco(funcproto(pVal)));
				for (uint32_t i = 0; i < pVal->l.nupvalues; i++)
					info.AddNext(obj2gco(&gcref(pVal->l.uvptr[i])->uv));
			} else {
				for (uint32_t i = 0; i < pVal->c.nupvalues; i++)
				{
					TValue* pTV = &pVal->c.upvalue[i];
					if (tvisgcv(pTV))
						info.AddNext(gcV(pTV));
				}
			}
		}
		break;
	case ~LJ_TPROTO:
		{
			GCproto* pVal = gco2pt(pObj);
			info.AddNext(obj2gco(proto_chunkname(pVal)));

			for (ptrdiff_t i = -(ptrdiff_t)pVal->sizekgc; i < 0; i++)
				info.AddNext(proto_kgc(pVal, i));

			global_State* g = G(L);
			if (pVal->trace)
				info.AddNext(obj2gco(traceref(G2J(g), pVal->trace)));
		}
		break;
	case ~LJ_TTRACE:
		{
			GCtrace* pVal = gco2trace(pObj);
			info.AddNext(gcref(pVal->startpt));

			IRRef ref;
			if (pVal->traceno == 0)
				break;
			
			for (ref = pVal->nk; ref < REF_TRUE; ref++) {
				IRIns *ir = &pVal->ir[ref];
				if (ir->o == IR_KGC)
					info.AddNext(ir_kgc(ir));

				if (irt_is64(ir->t) && ir->o != IR_KNULL)
					ref++;
			}

			global_State* g = G(L);
			if (pVal->nextside)
				info.AddNext(obj2gco(traceref(G2J(g), pVal->nextside)));

			if (pVal->link)
				info.AddNext(obj2gco(traceref(G2J(g), pVal->link)));

			if (pVal->nextroot)
				info.AddNext(obj2gco(traceref(G2J(g), pVal->nextroot)));
		}
		break;
	case ~LJ_TTHREAD:
		{
			lua_State* pVal = gco2th(pObj);
			info.AddNext(gcref(pVal->env));

			GCupval* pUpVal = gcref(pVal->openupval) ? gco2uv(gcref(pVal->openupval)) : NULL;
			while (pUpVal)
			{
				TValue* pUpValTV = uvval(pUpVal);
				if (pUpValTV && tvisgcv(pUpValTV))
					info.AddNext(obj2gco(gcV(pUpValTV)));

				pUpVal = uvnext(pUpVal);
			}

			TValue* pBase = Lua::LuaBase(pVal);
			int nTop = (int)(Lua::LuaTop(pVal) - pBase);
			for (int i=0; i<nTop; ++i)
			{
				if (tvisgcv(pBase))
					info.AddNext(gcval(pBase));

				pBase++;
			}
		}
		break;
	default:
		break;
	}
}

LUA_FUNCTION_STATIC(luagc_GetContainingReferences)
{
	TraverseInfo info;

	bool bRecursive = LUA->GetBool(2);
	lua_State* L = LUA->GetState();
	if (LUA->IsType(3, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(3);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			TValue* pVal = Lua::index2adr(L, -1);
			if (tvisgcv(pVal))
				info.m_Visited.insert(gcV(pVal));

			LUA->Pop(1);
		}
		LUA->Pop(1);
	}

	TValue* pVal = Lua::index2adr(L, 1);
	if (tvisgcv(pVal))
	{
		LuaGC_References(gcV(pVal), info, L, LUA);

		if (bRecursive)
		{
			GCobj* pNextObj = info.GetNext();
			while (pNextObj)
			{
				LuaGC_References(pNextObj, info, L, LUA);
				pNextObj = info.GetNext();
			}
		}
	}

	int nCount = 0;
	LUA->PreCreateTable(info.m_Visited.size(), 0);
	for (GCobj* pObj : info.m_Visited)
	{
		LUA->PushNil();
		setgcV(L, Lua::LuaTop(L)-1, pObj, ~pObj->gch.gct);
		Util::RawSetI(LUA, -2, ++nCount);
	}

	return 1;
}

LUA_FUNCTION_STATIC(luagc_GetAllGCObjects)
{
	lua_State* L = LUA->GetState();

	LUA->PreCreateTable(0, 0);
	global_State* pGState = G(L);
	if (!pGState)
		return 1;

	// Allows you to pass this function an GC object
	// This causes us to stop when we reach it
	GCobj* pTargetObject = nullptr;
	TValue* pVal = Lua::index2adr(L, 1);
	if (tvisgcv(pVal))
		pTargetObject = gcV(pVal);

	int nCount = 0;
	GCobj* pObj = gcref(pGState->gc.root);
	while (pObj && pObj != pTargetObject)
	{
		LUA->PushNil();
		setgcV(L, Lua::LuaTop(L)-1, pObj, ~pObj->gch.gct);
		Util::RawSetI(LUA, -2, ++nCount);

		pObj = gcref(pObj->gch.nextgc);
	}

	return 1;
}

LUA_FUNCTION_STATIC(luagc_GetCurrentGCHeadObject)
{
	lua_State* L = LUA->GetState();
	global_State* pGState = G(L);
	if (!pGState)
	{
		LUA->PushNil();
		return 1;
	}

	LUA->PushNil();
	GCobj* pObj = gcref(pGState->gc.root);
	if (pObj)
		setgcV(L, Lua::LuaTop(L)-1, pObj, ~pObj->gch.gct);

	return 1;
}

static inline void PushGCObject(GarrysMod::Lua::ILuaInterface* LUA, GCobj* pObj)
{
	lua_State* L = LUA->GetState();

	LUA->PushNil();
	if (pObj)
		setgcV(L, Lua::LuaTop(L)-1, pObj, ~pObj->gch.gct);
}

static inline void PushGCTypeName(GarrysMod::Lua::ILuaInterface* LUA, const char* pName)
{
	LUA->PushString("type");
	LUA->PushString(pName);
	LUA->RawSet(-3);
}

static size_t LuaGC_RecursiveSize(GCobj* pObj, TraverseInfo& info, lua_State* L, bool bRecursive);
static void LuaGC_ShowReferences(GarrysMod::Lua::ILuaInterface* LUA, GCobj* pObj)
{
	if (!pObj)
		return;

	LUA->PreCreateTable(0, 2);

	LUA->PushString("object");
	PushGCObject(LUA, pObj);
	LUA->RawSet(-3);

	{
		unordered_set<GCobj*> nWalkedObjects;
		LUA->PushString("size");
		TraverseInfo info;
		LUA->PushNumber(LuaGC_RecursiveSize(pObj, info, LUA->GetState(), false));
		LUA->RawSet(-3);
	}

	switch(pObj->gch.gct)
	{
	case ~LJ_TUPVAL:
		{
			PushGCTypeName(LUA, "upvalue");

			GCupval* pVal = gco2uv(pObj);
			TValue* pTV = uvval(pVal);
			if (pTV && tvisgcv(pTV))
			{
				LUA->PushString("value");
				PushGCObject(LUA, gcV(pTV));
				LUA->RawSet(-3);
			}
		}
		return;
	case ~LJ_TUDATA:
		{
			PushGCTypeName(LUA, "userdata");

			GCudata* pVal = gco2ud(pObj);
			LUA->PushString("environment");
			PushGCObject(LUA, gcref(pVal->env));
			LUA->RawSet(-3);

			LUA->PushString("metatable");
			PushGCObject(LUA, gcref(pVal->metatable));
			LUA->RawSet(-3);
		}
		return;
	case ~LJ_TTAB:
		{
			PushGCTypeName(LUA, "table");

			GCtab* pVal = gco2tab(pObj);
			LUA->PushString("metatable");
			PushGCObject(LUA, gcref(pVal->metatable));
			LUA->RawSet(-3);

			
			MSize i, asize = pVal->asize;
			if (asize)
			{
				LUA->PushString("arraySlots");
				LUA->PreCreateTable(asize, 0);
				int nCount = 0;
				for (i = 0; i < asize; i++)
				{
					TValue* pTV = arrayslot(pVal, i);
					if (tvisgcv(pTV))
					{
						PushGCObject(LUA, gcV(pTV));
						Util::RawSetI(LUA, -2, ++nCount);
					}
				}
				LUA->RawSet(-3);
			}

			if (pVal->hmask > 0)
			{
				LUA->PushString("hashSlots");
				Node *node = noderef(pVal->node);
				MSize hmask = pVal->hmask;
				LUA->PreCreateTable(hmask, 0);
				int nCount = 0;
				for (i = 0; i <= hmask; i++)
				{
					Node *n = &node[i];
					if (!tvisnil(&n->val))
					{
						LUA->PreCreateTable(0, 2);
						
						if (tvisgcv(&n->key))
						{
							LUA->PushString("key");
							PushGCObject(LUA, gcV(&n->key));
							LUA->RawSet(-3);
						}

						if (tvisgcv(&n->val))
						{
							LUA->PushString("value");
							PushGCObject(LUA, gcV(&n->val));
							LUA->RawSet(-3);
						}

						Util::RawSetI(LUA, -2, ++nCount);
					}
				}
				LUA->RawSet(-3);
			}
		}
		return;
	case ~LJ_TFUNC:
		{
			PushGCTypeName(LUA, "function");

			GCfunc* pVal = gco2func(pObj);
			LUA->PushString("environment");
			PushGCObject(LUA, gcref(pVal->c.env));
			LUA->RawSet(-3);

			if (isluafunc(pVal))
			{
				LUA->PushString("proto");
				PushGCObject(LUA, obj2gco(funcproto(pVal)));
				LUA->RawSet(-3);
				
				LUA->PushString("upvalues");
				LUA->PreCreateTable(0, 0);
				int nCount = 0;
				for (uint32_t i = 0; i < pVal->l.nupvalues; i++)
				{
					PushGCObject(LUA, obj2gco(&gcref(pVal->l.uvptr[i])->uv));
					Util::RawSetI(LUA, -2, ++nCount);
				}
				LUA->RawSet(-3);
			} else {
				LUA->PushString("upvalues");
				LUA->PreCreateTable(0, 0);
				int nCount = 0;
				for (uint32_t i = 0; i < pVal->c.nupvalues; i++)
				{
					TValue* pTV = &pVal->c.upvalue[i];
					if (tvisgcv(pTV))
					{
						PushGCObject(LUA, gcV(pTV));
						Util::RawSetI(LUA, -2, ++nCount);
					}
				}
				LUA->RawSet(-3);
			}
		}
		return;
	case ~LJ_TPROTO:
		{
			PushGCTypeName(LUA, "proto");

			GCproto* pVal = gco2pt(pObj);
			LUA->PushString("name");
			PushGCObject(LUA, obj2gco(proto_chunkname(pVal)));
			LUA->RawSet(-3);

			LUA->PushString("constants");
			LUA->PreCreateTable(0, 0);
			int nCount = 0;
			for (ptrdiff_t i = -(ptrdiff_t)pVal->sizekgc; i < 0; i++)
			{
				PushGCObject(LUA, proto_kgc(pVal, i));
				Util::RawSetI(LUA, -2, ++nCount);
			}
			LUA->RawSet(-3);

			global_State* g = G(LUA->GetState());
			if (pVal->trace)
			{
				LUA->PushString("trace");
				PushGCObject(LUA, obj2gco(traceref(G2J(g), pVal->trace)));
				LUA->RawSet(-3);
			}
		}
		return;
	case ~LJ_TTRACE:
		{
			PushGCTypeName(LUA, "trace");

			GCtrace* pVal = gco2trace(pObj);
			LUA->PushString("startproto");
			PushGCObject(LUA, gcref(pVal->startpt));
			LUA->RawSet(-3);

			IRRef ref;
			if (pVal->traceno == 0)
				break;
			
			LUA->PushString("constants");
			LUA->PreCreateTable(0, 0);
			int nCount = 0;
			for (ref = pVal->nk; ref < REF_TRUE; ref++) {
				IRIns *ir = &pVal->ir[ref];
				if (ir->o == IR_KGC)
				{
					PushGCObject(LUA, ir_kgc(ir));
					Util::RawSetI(LUA, -2, ++nCount);
				}

				if (irt_is64(ir->t) && ir->o != IR_KNULL)
					ref++;
			}
			LUA->RawSet(-3);

			global_State* g = G(LUA->GetState());
			if (pVal->nextside)
			{
				LUA->PushString("nextside");
				PushGCObject(LUA, obj2gco(traceref(G2J(g), pVal->nextside)));
				LUA->RawSet(-3);
			}

			if (pVal->link)
			{
				LUA->PushString("link");
				PushGCObject(LUA, obj2gco(traceref(G2J(g), pVal->link)));
				LUA->RawSet(-3);
			}

			if (pVal->nextroot)
			{
				LUA->PushString("nextroot");
				PushGCObject(LUA, obj2gco(traceref(G2J(g), pVal->nextroot)));
				LUA->RawSet(-3);
			}
		}
		return;
	case ~LJ_TTHREAD:
		{
			PushGCTypeName(LUA, "thread");

			lua_State* pVal = gco2th(pObj);
			LUA->PushString("environment");
			PushGCObject(LUA, gcref(pVal->env));
			LUA->RawSet(-3);

			LUA->PushString("upvalues");
			LUA->PreCreateTable(0, 0);
			int nCount = 0;
			GCupval* pUpVal = gcref(pVal->openupval) ? gco2uv(gcref(pVal->openupval)) : NULL;
			while (pUpVal)
			{
				TValue* pUpValTV = uvval(pUpVal);
				if (pUpValTV && tvisgcv(pUpValTV))
				{
					PushGCObject(LUA, obj2gco(gcV(pUpValTV)));
					Util::RawSetI(LUA, -2, ++nCount);
				}

				pUpVal = uvnext(pUpVal);
			}
			LUA->RawSet(-3);
			
			LUA->PushString("stack");
			LUA->PreCreateTable(0, 0);
			nCount = 0;
			TValue* pBase = Lua::LuaBase(pVal);
			int nTop = (int)(Lua::LuaTop(pVal) - pBase);
			for (int i=0; i<nTop; ++i)
			{
				if (tvisgcv(pBase))
				{
					PushGCObject(LUA, gcval(pBase));
					Util::RawSetI(LUA, -2, ++nCount);
				}

				pBase++;
			}
			LUA->RawSet(-3);
		}
		return;
	default:
		break;
	}
}

LUA_FUNCTION_STATIC(luagc_GetFormattedGCObjectInfo)
{
	TValue* pVal = Lua::index2adr(LUA->GetState(), 1);
	if (!tvisgcv(pVal))
	{
		LUA->PushNil();
		return 1;
	}

	LuaGC_ShowReferences(LUA, gcV(pVal));

	return 1;
}

static size_t LuaGC_Size(GCobj* pObj, TraverseInfo& info, lua_State* L)
{
	if (!pObj)
		return 0;

	size_t nSize = 0;
	switch(pObj->gch.gct)
	{
	case ~LJ_TUPVAL:
		{
			GCupval* pVal = gco2uv(pObj);
			nSize += sizeof(GCupval);

			TValue* pTV = uvval(pVal);
			if (pTV && tvisgcv(pTV))
				info.AddNext(gcV(pTV));
		}
		break;
	case ~LJ_TUDATA:
		{
			GCudata* pVal = gco2ud(pObj);
			nSize += sizeudata(pVal);

			info.AddNext(gcref(pVal->env));
			info.AddNext(gcref(pVal->metatable));
		}
		break;
	case ~LJ_TTAB:
		{
			GCtab* pVal = gco2tab(pObj);
			
			if (pVal->hmask > 0)
				nSize += (pVal->hmask+1) * sizeof(Node);

			if (pVal->asize > 0 && LJ_MAX_COLOSIZE != 0 && pVal->colo <= 0)
				nSize += pVal->asize * sizeof(TValue);

			if (LJ_MAX_COLOSIZE != 0 && pVal->colo)
				nSize += sizetabcolo((uint32_t)pVal->colo & 0x7f);
			else
				nSize += sizeof(GCtab);

			info.AddNext(gcref(pVal->metatable));

			MSize i, asize = pVal->asize;
			for (i = 0; i < asize; i++)
			{
				TValue* pTV = arrayslot(pVal, i);
				if (tvisgcv(pTV))
					info.AddNext(gcV(pTV));
			}

			if (pVal->hmask > 0)
			{
				Node *node = noderef(pVal->node);
				MSize hmask = pVal->hmask;
				for (i = 0; i <= hmask; i++)
				{
					Node *n = &node[i];
					if (!tvisnil(&n->val))
					{
						if (tvisgcv(&n->key))
							info.AddNext(gcV(&n->key));

						if (tvisgcv(&n->val))
							info.AddNext(gcV(&n->val));
					}
				}
			}
		}
		break;
	case ~LJ_TFUNC:
		{
			GCfunc* pVal = gco2func(pObj);
			info.AddNext(gcref(pVal->c.env));
			
			if (isluafunc(pVal))
			{
				nSize += sizeLfunc((MSize)pVal->l.nupvalues);
				info.AddNext(obj2gco(funcproto(pVal)));
				
				for (uint32_t i = 0; i < pVal->l.nupvalues; i++)
					info.AddNext(obj2gco(&gcref(pVal->l.uvptr[i])->uv));
			} else {
				nSize += sizeCfunc((MSize)pVal->c.nupvalues);
				for (uint32_t i = 0; i < pVal->c.nupvalues; i++)
				{
					TValue* pTV = &pVal->c.upvalue[i];
					if (tvisgcv(pTV))
						info.AddNext(gcV(pTV));
				}
			}
		}
		break;
	case ~LJ_TPROTO:
		{
			GCproto* pVal = gco2pt(pObj);
			nSize += pVal->sizept; // Includes sizeof(GCproto) already!
			info.AddNext(obj2gco(proto_chunkname(pVal)));

			for (ptrdiff_t i = -(ptrdiff_t)pVal->sizekgc; i < 0; i++)
				info.AddNext(proto_kgc(pVal, i));

			global_State* g = G(L);
			if (pVal->trace)
				info.AddNext(obj2gco(traceref(G2J(g), pVal->trace)));
		}
		break;
	case ~LJ_TTRACE:
		{
			GCtrace* pVal = gco2trace(pObj);
			info.AddNext(gcref(pVal->startpt));

			if (pVal->traceno)
			{
#if LUAJIT_USE_GDBJIT
				GDBJITentryobj *eo = (GDBJITentryobj*)pVal->gdbjit_entry;
				nSize += eo->sz;
#endif
			}
			
			nSize += ((sizeof(GCtrace)+7)&~7) + (pVal->nins-pVal->nk)*sizeof(IRIns) + pVal->nsnap*sizeof(SnapShot) + pVal->nsnapmap*sizeof(SnapEntry);

			IRRef ref;
			if (pVal->traceno == 0)
				break;
			
			for (ref = pVal->nk; ref < REF_TRUE; ref++) {
				IRIns *ir = &pVal->ir[ref];
				if (ir->o == IR_KGC)
					info.AddNext(ir_kgc(ir));

				if (irt_is64(ir->t) && ir->o != IR_KNULL)
					ref++;
			}

			global_State* g = G(L);
			if (pVal->nextside)
				info.AddNext(obj2gco(traceref(G2J(g), pVal->nextside)));

			if (pVal->link)
				info.AddNext(obj2gco(traceref(G2J(g), pVal->link)));

			if (pVal->nextroot)
				info.AddNext(obj2gco(traceref(G2J(g), pVal->nextroot)));
		}
		break;
	case ~LJ_TTHREAD:
		{
			lua_State* pVal = gco2th(pObj);
			nSize += sizeof(lua_State);
			nSize += pVal->stacksize * sizeof(TValue);

			info.AddNext(gcref(pVal->env));

			GCupval* pUpVal = gcref(pVal->openupval) ? gco2uv(gcref(pVal->openupval)) : NULL;
			while (pUpVal)
			{
				TValue* pUpValTV = uvval(pUpVal);
				if (pUpValTV)
				{
					if (tvisgcv(pUpValTV))
						info.AddNext(obj2gco(gcV(pUpValTV)));
					else
						nSize += sizeof(TValue);
				}

				pUpVal = uvnext(pUpVal);
			}

			TValue* pBase = Lua::LuaBase(pVal);
			int nTop = (int)(Lua::LuaTop(pVal) - pBase);
			for (int i=0; i<nTop; ++i)
			{
				if (tvisgcv(pBase))
					info.AddNext(gcval(pBase));

				pBase++;
			}
		}
		break;
	case ~LJ_TCDATA:
		{
			// Gmod has no FFI so we'll use our JIT version since it must be the luajit module's JIT version then
			nSize += RawLua::GetCDataSize(L, gco2cd(pObj));
		}
		break;
	case ~LJ_TSTR:
		nSize += sizestring(gco2str(pObj));
		break;
	default:
		break;
	}

	return nSize;
}

static size_t LuaGC_RecursiveSize(GCobj* pObj, TraverseInfo& info, lua_State* L, bool bRecursive)
{
	size_t size = LuaGC_Size(pObj, info, L);

	if (!bRecursive)
		info.Lock();

	GCobj* pNextObj = info.GetNext();
	while (pNextObj)
	{
		size += LuaGC_Size(pNextObj, info, L);
		pNextObj = info.GetNext();
	}

	return size;
}

LUA_FUNCTION_STATIC(luagc_GetSizeOfGCObject)
{
	lua_State* L = LUA->GetState();
	bool bRecursive = LUA->GetBool(2);
	TraverseInfo info;
	if (LUA->IsType(3, GarrysMod::Lua::Type::Table))
	{
		LUA->Push(3);
		LUA->PushNil();
		while (LUA->Next(-2))
		{
			TValue* pVal = Lua::index2adr(L, -1);
			if (tvisgcv(pVal))
				info.m_Visited.insert(gcV(pVal));

			LUA->Pop(1);
		}
		LUA->Pop(1);
	}

	TValue* pVal = Lua::index2adr(L, 1);
	if (tvisgcv(pVal))
		LUA->PushNumber(LuaGC_RecursiveSize(gcV(pVal), info, LUA->GetState(), bRecursive));
	else
		LUA->PushNumber(0);
	
	return 1;
}

//HOLYLIB_SETUP_FILE_END



#if MODULE_EXISTS_LUAJIT
LUA_FUNCTION_EXTERN(luajit_luagc_GetGCCount)
LUA_FUNCTION_EXTERN(luajit_luagc_GetReferences)
LUA_FUNCTION_EXTERN(luajit_luagc_GetContainingReferences)
LUA_FUNCTION_EXTERN(luajit_luagc_GetAllGCObjects)
LUA_FUNCTION_EXTERN(luajit_luagc_GetCurrentGCHeadObject)
LUA_FUNCTION_EXTERN(luajit_luagc_GetFormattedGCObjectInfo)
LUA_FUNCTION_EXTERN(luajit_luagc_GetSizeOfGCObject)
#endif

void CLuaGCModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
#if MODULE_EXISTS_LUAJIT
		IModuleWrapper* pWrapper = g_pModuleManager.GetModuleByID(pLuaJITModule->m_pID);
		if (pWrapper && pWrapper->IsEnabled())
		{
			Util::AddFunc(pLua, luajit_luagc_GetGCCount, "GetGCCount"); // GCobj count
			Util::AddFunc(pLua, luajit_luagc_GetReferences, "GetReferences"); // A table containing all GCobjs that reference the given GCobj
			Util::AddFunc(pLua, luajit_luagc_GetContainingReferences, "GetContainingReferences"); // A table containing all GCobjs the given object references recursively
			Util::AddFunc(pLua, luajit_luagc_GetAllGCObjects, "GetAllGCObjects"); // All GCobjs
			Util::AddFunc(pLua, luajit_luagc_GetCurrentGCHeadObject, "GetCurrentGCHeadObject");
			Util::AddFunc(pLua, luajit_luagc_GetFormattedGCObjectInfo, "GetFormattedGCObjectInfo");
			Util::AddFunc(pLua, luajit_luagc_GetSizeOfGCObject, "GetSizeOfGCObject");
		} else
#endif
		{
			Util::AddFunc(pLua, luagc_GetGCCount, "GetGCCount"); // GCobj count
			Util::AddFunc(pLua, luagc_GetReferences, "GetReferences"); // A table containing all GCobjs that reference the given GCobj
			Util::AddFunc(pLua, luagc_GetContainingReferences, "GetContainingReferences"); // A table containing all GCobjs the given object references recursively
			Util::AddFunc(pLua, luagc_GetAllGCObjects, "GetAllGCObjects"); // All GCobjs
			Util::AddFunc(pLua, luagc_GetCurrentGCHeadObject, "GetCurrentGCHeadObject");
			Util::AddFunc(pLua, luagc_GetFormattedGCObjectInfo, "GetFormattedGCObjectInfo");
			Util::AddFunc(pLua, luagc_GetSizeOfGCObject, "GetSizeOfGCObject");
		}
	Util::FinishTable(pLua, "luagc");
}

void CLuaGCModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "luagc");
}

void CLuaGCModule::Shutdown()
{
}