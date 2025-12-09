#include "module.h"
#include "LuaInterface.h"
#include "lua.h"

#if PLATFORM_64BITS
#include "../gmod-luajit/luajit21/lj_jit.h"
#include "../gmod-luajit/luajit21/lj_dispatch.h"
#include "../gmod-luajit/luajit21/lj_tab.h"
#else
#include "../gmod-luajit/luajit20/lj_jit.h"
#include "../gmod-luajit/luajit20/lj_dispatch.h"
#include "../gmod-luajit/luajit20/lj_tab.h"
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

LUA_FUNCTION_STATIC(luagc_GetGCCount)
{
	global_State* pGState = G(LUA->GetState());
	if (!pGState)
	{
		LUA->PushNumber(0);
		return 1;
	}

	int nCount = 0;
	GCobj* pObj = gcref(pGState->gc.root);
	while (pObj)
	{
		++nCount;
		pObj = gcref(pObj->gch.nextgc);
	}

	LUA->PushNumber(nCount);
	return 1;
}

bool LuaGC_WalkReferenceCheck(GCobj* pTargetObj, GCobj* pObj, lua_State* L, bool bChild)
{
	if (!pObj)
		return false;

	if (bChild)
		return pTargetObj == pObj;

	switch(pObj->gch.gct)
	{
	case ~LJ_TUPVAL:
		{
			GCupval* pVal = gco2uv(pObj);

			TValue* pTV = uvval(pVal);
			if (pTV && tvisgcv(pTV))
				if (LuaGC_WalkReferenceCheck(pTargetObj, gcV(pTV), L, true))
					return true;
		}
		break;
	case ~LJ_TUDATA:
		{
			GCudata* pVal = gco2ud(pObj);
			if (LuaGC_WalkReferenceCheck(pTargetObj, gcref(pVal->env), L, true))
				return true;

			if (LuaGC_WalkReferenceCheck(pTargetObj, gcref(pVal->metatable), L, true))
				return true;
		}
		break;
	case ~LJ_TTAB:
		{
			GCtab* pVal = gco2tab(pObj);
			if (LuaGC_WalkReferenceCheck(pTargetObj, gcref(pVal->metatable), L, true))
				return true;

			MSize i, asize = pVal->asize;
			for (i = 0; i < asize; i++)
			{
				TValue* pTV = arrayslot(pVal, i);
				if (tvisgcv(pTV))
					if (LuaGC_WalkReferenceCheck(pTargetObj, gcV(pTV), L, true))
						return true;
			}

			if (pVal->hmask > 0)
			{
				Node *node = noderef(pVal->node);
				MSize i, hmask = pVal->hmask;
				for (i = 0; i <= hmask; i++)
				{
					Node *n = &node[i];
					if (!tvisnil(&n->val))
					{
						if (tvisgcv(&n->key))
							if (LuaGC_WalkReferenceCheck(pTargetObj, gcV(&n->key), L, true))
								return true;

						if (tvisgcv(&n->val))
							if (LuaGC_WalkReferenceCheck(pTargetObj, gcV(&n->val), L, true))
								return true;
					}
				}
			}
		}
		break;
	case ~LJ_TFUNC:
		{
			GCfunc* pVal = gco2func(pObj);
			if (LuaGC_WalkReferenceCheck(pTargetObj, gcref(pVal->c.env), L, true))
				return true;

			if (isluafunc(pVal))
			{
				if (LuaGC_WalkReferenceCheck(pTargetObj, obj2gco(funcproto(pVal)), L, true))
					return true;

				for (uint32_t i = 0; i < pVal->l.nupvalues; i++)
					if (LuaGC_WalkReferenceCheck(pTargetObj, obj2gco(&gcref(pVal->l.uvptr[i])->uv), L, true))
						return true;
			} else {
				for (uint32_t i = 0; i < pVal->c.nupvalues; i++)
				{
					TValue* pTV = &pVal->c.upvalue[i];
					if (tvisgcv(pTV))
						if (LuaGC_WalkReferenceCheck(pTargetObj, gcV(pTV), L, true))
							return true;
				}
			}
		}
		break;
	case ~LJ_TPROTO:
		{
			GCproto* pVal = gco2pt(pObj);
			if (LuaGC_WalkReferenceCheck(pTargetObj, obj2gco(proto_chunkname(pVal)), L, true))
				return true;

			for (ptrdiff_t i = -(ptrdiff_t)pVal->sizekgc; i < 0; i++)
				if (LuaGC_WalkReferenceCheck(pTargetObj, proto_kgc(pVal, i), L, true))
					return true;

			global_State* g = G(L);
			if (pVal->trace)
				if (LuaGC_WalkReferenceCheck(pTargetObj, obj2gco(traceref(G2J(g), pVal->trace)), L, true))
					return true;
		}
		break;
	case ~LJ_TTRACE:
		{
			GCtrace* pVal = gco2trace(pObj);
			if (LuaGC_WalkReferenceCheck(pTargetObj, gcref(pVal->startpt), L, true))
				return true;

			IRRef ref;
			if (pVal->traceno == 0)
				break;
			
			for (ref = pVal->nk; ref < REF_TRUE; ref++) {
				IRIns *ir = &pVal->ir[ref];
				if (ir->o == IR_KGC)
					if (LuaGC_WalkReferenceCheck(pTargetObj, ir_kgc(ir), L, true))
						return true;

				if (irt_is64(ir->t) && ir->o != IR_KNULL)
					ref++;
			}

			global_State* g = G(L);
			if (pVal->nextside)
				if (LuaGC_WalkReferenceCheck(pTargetObj, obj2gco(traceref(G2J(g), pVal->nextside)), L, true))
					return true;

			if (pVal->link)
				if (LuaGC_WalkReferenceCheck(pTargetObj, obj2gco(traceref(G2J(g), pVal->link)), L, true))
					return true;

			if (pVal->nextroot)
				if (LuaGC_WalkReferenceCheck(pTargetObj, obj2gco(traceref(G2J(g), pVal->nextside)), L, true))
					return true;
		}
		break;
	case ~LJ_TTHREAD:
		{
			lua_State* pVal = gco2th(pObj);
			if (LuaGC_WalkReferenceCheck(pTargetObj, gcref(pVal->env), L, true))
				return true;

			GCupval* pUpVal = gco2uv(gcref(pVal->openupval));
			while (pUpVal)
			{
				if (LuaGC_WalkReferenceCheck(pTargetObj, obj2gco(gcV(uvval(pUpVal))), L, true))
					return true;

				pUpVal = uvnext(pUpVal);
			}

			TValue* pBase = pVal->base;
			int nTop = (int)(pVal->top - pVal->base);
			for (int i=0; i<nTop; ++i)
			{
				if (tvisgcv(pBase))
					if (LuaGC_WalkReferenceCheck(pTargetObj, gcval(pBase), L, true))
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
	std::unordered_set<GCobj*> nIterated;
	LUA->PreCreateTable(0, 0);

	lua_State* L = LUA->GetState();
	global_State* pGState = G(L);
	if (!pGState)
		return 1;

	TValue* pVal = RawLua::index2adr(L, 1);
	if (!tvisgcv(pVal))
		return 1;

	GCobj* pTargetObject = gcV(pVal);
	int nCount = 0;
	GCobj* pObj = gcref(pGState->gc.root);
	while (pObj)
	{
		if (LuaGC_WalkReferenceCheck(pTargetObject, pObj, L, false))
		{
			LUA->PushNil();
			setgcV(L, L->top-1, pObj, ~pObj->gch.gct);
			Util::RawSetI(LUA, -2, ++nCount);
		}

		pObj = gcref(pObj->gch.nextgc);
	}

	return 1;
}

extern void LuaGC_WalkReferences(GCobj* pObj, std::unordered_set<GCobj*>& nWalkedObjects, int& nCount, lua_State* L, GarrysMod::Lua::ILuaInterface* LUA, bool bIsChild, bool bRecursive);
void LuaGC_WalkReferences(GCobj* pObj, std::unordered_set<GCobj*>& nWalkedObjects, int& nCount, lua_State* L, GarrysMod::Lua::ILuaInterface* LUA, bool bIsChild, bool bRecursive)
{
	if (!pObj || nWalkedObjects.find(pObj) != nWalkedObjects.end())
		return;

	nWalkedObjects.insert(pObj);

	// Top of the stack MUST have a table
	LUA->PushNil();
	setgcV(L, L->top-1, pObj, ~pObj->gch.gct);
	Util::RawSetI(LUA, -2, ++nCount);

	if (bIsChild && !bRecursive)
		return;

	switch(pObj->gch.gct)
	{
	case ~LJ_TUPVAL:
		{
			GCupval* pVal = gco2uv(pObj);

			TValue* pTV = uvval(pVal);
			if (pTV && tvisgcv(pTV))
				LuaGC_WalkReferences(gcV(pTV), nWalkedObjects, nCount, L, LUA, true, bRecursive);
		}
		break;
	case ~LJ_TUDATA:
		{
			GCudata* pVal = gco2ud(pObj);
			LuaGC_WalkReferences(gcref(pVal->env), nWalkedObjects, nCount, L, LUA, true, bRecursive);
			LuaGC_WalkReferences(gcref(pVal->metatable), nWalkedObjects, nCount, L, LUA, true, bRecursive);
		}
		break;
	case ~LJ_TTAB:
		{
			GCtab* pVal = gco2tab(pObj);
			LuaGC_WalkReferences(gcref(pVal->metatable), nWalkedObjects, nCount, L, LUA, true, bRecursive);

			MSize i, asize = pVal->asize;
			for (i = 0; i < asize; i++)
			{
				TValue* pTV = arrayslot(pVal, i);
				if (tvisgcv(pTV))
					LuaGC_WalkReferences(gcV(pTV), nWalkedObjects, nCount, L, LUA, true, bRecursive);
			}

			if (pVal->hmask > 0)
			{
				Node *node = noderef(pVal->node);
				MSize i, hmask = pVal->hmask;
				for (i = 0; i <= hmask; i++)
				{
					Node *n = &node[i];
					if (!tvisnil(&n->val))
					{
						if (tvisgcv(&n->key))
							LuaGC_WalkReferences(gcV(&n->key), nWalkedObjects, nCount, L, LUA, true, bRecursive);

						if (tvisgcv(&n->val))
							LuaGC_WalkReferences(gcV(&n->val), nWalkedObjects, nCount, L, LUA, true, bRecursive);
					}
				}
			}
		}
		break;
	case ~LJ_TFUNC:
		{
			GCfunc* pVal = gco2func(pObj);
			LuaGC_WalkReferences(gcref(pVal->c.env), nWalkedObjects, nCount, L, LUA, true, bRecursive);
			if (isluafunc(pVal))
			{
				LuaGC_WalkReferences(obj2gco(funcproto(pVal)), nWalkedObjects, nCount, L, LUA, true, bRecursive);
				for (uint32_t i = 0; i < pVal->l.nupvalues; i++)
					LuaGC_WalkReferences(obj2gco(&gcref(pVal->l.uvptr[i])->uv), nWalkedObjects, nCount, L, LUA, true, bRecursive);
			} else {
				for (uint32_t i = 0; i < pVal->c.nupvalues; i++)
				{
					TValue* pTV = &pVal->c.upvalue[i];
					if (tvisgcv(pTV))
						LuaGC_WalkReferences(gcV(pTV), nWalkedObjects, nCount, L, LUA, true, bRecursive);
				}
			}
		}
		break;
	case ~LJ_TPROTO:
		{
			GCproto* pVal = gco2pt(pObj);
			LuaGC_WalkReferences(obj2gco(proto_chunkname(pVal)), nWalkedObjects, nCount, L, LUA, true, bRecursive);

			for (ptrdiff_t i = -(ptrdiff_t)pVal->sizekgc; i < 0; i++)
				LuaGC_WalkReferences(proto_kgc(pVal, i), nWalkedObjects, nCount, L, LUA, true, bRecursive);

			global_State* g = G(L);
			if (pVal->trace)
				LuaGC_WalkReferences(obj2gco(traceref(G2J(g), pVal->trace)), nWalkedObjects, nCount, L, LUA, true, bRecursive);
		}
		break;
	case ~LJ_TTRACE:
		{
			GCtrace* pVal = gco2trace(pObj);
			LuaGC_WalkReferences(gcref(pVal->startpt), nWalkedObjects, nCount, L, LUA, true, bRecursive);

			IRRef ref;
			if (pVal->traceno == 0)
				break;
			
			for (ref = pVal->nk; ref < REF_TRUE; ref++) {
				IRIns *ir = &pVal->ir[ref];
				if (ir->o == IR_KGC)
					LuaGC_WalkReferences(ir_kgc(ir), nWalkedObjects, nCount, L, LUA, true, bRecursive);

				if (irt_is64(ir->t) && ir->o != IR_KNULL)
					ref++;
			}

			global_State* g = G(L);
			if (pVal->nextside)
				LuaGC_WalkReferences(obj2gco(traceref(G2J(g), pVal->nextside)), nWalkedObjects, nCount, L, LUA, true, bRecursive);

			if (pVal->link)
				LuaGC_WalkReferences(obj2gco(traceref(G2J(g), pVal->link)), nWalkedObjects, nCount, L, LUA, true, bRecursive);

			if (pVal->nextroot)
				LuaGC_WalkReferences(obj2gco(traceref(G2J(g), pVal->nextside)), nWalkedObjects, nCount, L, LUA, true, bRecursive);
		}
		break;
	case ~LJ_TTHREAD:
		{
			lua_State* pVal = gco2th(pObj);
			LuaGC_WalkReferences(gcref(pVal->env), nWalkedObjects, nCount, L, LUA, true, bRecursive);

			GCupval* pUpVal = gco2uv(gcref(pVal->openupval));
			while (pUpVal)
			{
				LuaGC_WalkReferences(obj2gco(gcV(uvval(pUpVal))), nWalkedObjects, nCount, L, LUA, true, bRecursive);

				pUpVal = uvnext(pUpVal);
			}

			TValue* pBase = pVal->base;
			int nTop = (int)(pVal->top - pVal->base);
			for (int i=0; i<nTop; ++i)
			{
				if (tvisgcv(pBase))
					LuaGC_WalkReferences(gcval(pBase), nWalkedObjects, nCount, L, LUA, true, bRecursive);

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
	std::unordered_set<GCobj*> nWalkedObjects;
	LUA->PreCreateTable(0, 0);

	bool bRecursive = LUA->GetBool(2);

	TValue* pVal = RawLua::index2adr(LUA->GetState(), 1);
	if (tvisgcv(pVal))
	{
		int nCount = 0;
		LuaGC_WalkReferences(gcV(pVal), nWalkedObjects, nCount, LUA->GetState(), LUA, false, bRecursive);
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
	TValue* pVal = RawLua::index2adr(L, 1);
	if (tvisgcv(pVal))
		pTargetObject = gcV(pVal);

	int nCount = 0;
	GCobj* pObj = gcref(pGState->gc.root);
	while (pObj && pObj != pTargetObject)
	{
		LUA->PushNil();
		setgcV(L, L->top-1, pObj, ~pObj->gch.gct);
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
		setgcV(L, L->top-1, pObj, ~pObj->gch.gct);

	return 1;
}

static inline void PushGCObject(GarrysMod::Lua::ILuaInterface* LUA, GCobj* pObj)
{
	lua_State* L = LUA->GetState();

	LUA->PushNil();
	setgcV(L, L->top-1, pObj, ~pObj->gch.gct);
}

static inline void PushGCTypeName(GarrysMod::Lua::ILuaInterface* LUA, const char* pName)
{
	LUA->PushString("type");
	LUA->PushString(pName);
	LUA->RawSet(-3);
}

static void LuaGC_ShowReferences(GarrysMod::Lua::ILuaInterface* LUA, GCobj* pObj)
{
	if (!pObj)
		return;

	LUA->PreCreateTable(0, 2);

	LUA->PushString("object");
	PushGCObject(LUA, pObj);
	LUA->RawSet(-3);

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
				MSize i, hmask = pVal->hmask;
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
					Util::RawSetI(LUA, -2, nCount);
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
						Util::RawSetI(LUA, -2, nCount);
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
			int nCount;
			for (ref = pVal->nk; ref < REF_TRUE; ref++) {
				IRIns *ir = &pVal->ir[ref];
				if (ir->o == IR_KGC)
				{
					PushGCObject(LUA, gcref(pVal->startpt));
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
				LUA->PushString("nextside");
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
			GCupval* pUpVal = gco2uv(gcref(pVal->openupval));
			while (pUpVal)
			{
				PushGCObject(LUA, obj2gco(gcV(uvval(pUpVal))));
				Util::RawSetI(LUA, -2, ++nCount);

				pUpVal = uvnext(pUpVal);
			}
			LUA->RawSet(-3);
			
			LUA->PushString("stack");
			LUA->PreCreateTable(0, 0);
			nCount = 0;
			TValue* pBase = pVal->base;
			int nTop = (int)(pVal->top - pVal->base);
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
	TValue* pVal = RawLua::index2adr(LUA->GetState(), 1);
	if (!tvisgcv(pVal))
	{
		LUA->PushNil();
		return 1;
	}

	LuaGC_ShowReferences(LUA, gcV(pVal));
	return 1;
}

void CLuaGCModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable(pLua);
		Util::AddFunc(pLua, luagc_GetGCCount, "GetGCCount"); // GCobj count
		Util::AddFunc(pLua, luagc_GetReferences, "GetReferences"); // A table containing all GCobjs that reference the given GCobj
		Util::AddFunc(pLua, luagc_GetContainingReferences, "GetContainingReferences"); // A table containing all GCobjs the given object references recursively
		Util::AddFunc(pLua, luagc_GetAllGCObjects, "GetAllGCObjects"); // All GCobjs
		Util::AddFunc(pLua, luagc_GetCurrentGCHeadObject, "GetCurrentGCHeadObject");
		Util::AddFunc(pLua, luagc_GetFormattedGCObjectInfo, "GetFormattedGCObjectInfo");
	Util::FinishTable(pLua, "luagc");
}

void CLuaGCModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	Util::NukeTable(pLua, "luagc");
}

void CLuaGCModule::Shutdown()
{
}