#include "LuaInterface.h"
#include "module.h"
#include "lua.h"
#include <chrono>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CSysTimerModule : public IModule
{
public:
	void LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit) override;
	void LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua) override;
	void LuaThink(GarrysMod::Lua::ILuaInterface* pLua) override;
	const char* Name() override { return "systimer"; };
	int Compatibility() override { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
	bool SupportsMultipleLuaStates() override { return true; };
};

static CSysTimerModule g_pSysTimerModule;
IModule* pSysTimerModule = &g_pSysTimerModule;

struct ILuaTimer
{
	~ILuaTimer()
	{
		if (function != -1)
			Util::ReferenceFree(pLua, function, "ILuaTimer::~ILuaTimer - function");

		if (identifierReference != -1)
			Util::ReferenceFree(pLua, identifierReference, "ILuaTimer::~ILuaTimer - identifyer");
	}

	// Should we try to make this struct smaller?
	float delay = 0;
	double nextRunTime = 0;
	unsigned int repetitions = 0;
	int function = -1;

	//We could make a separate struct for timer simple :^
	int identifierReference = -1;
	const char* identifier = nullptr;
	bool active = true;
	bool markDelete = false;
	GarrysMod::Lua::ILuaInterface* pLua;
};

static double GetTime()
{
	return (double)std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() / 1000;
}

class LuaSysTimerModuleData : public Lua::ModuleData
{
public:
	CUtlVector<ILuaTimer*> pLuaTimers;
};

LUA_GetModuleData(LuaSysTimerModuleData, g_pSysTimerModule, SysTimer)

static ILuaTimer* FindTimer(GarrysMod::Lua::ILuaInterface* pLua, const char* name) // We should probably use a set or so to have faster look up. But my precious memory :(
{
	auto pData = GetSysTimerLuaData(pLua);

	FOR_EACH_VEC(pData->pLuaTimers, i)
	{
		ILuaTimer* timer = pData->pLuaTimers[i];
		if (timer->identifier != nullptr && strcmp(name, timer->identifier) == 0)
		{
			return timer;
		}
	}

	return nullptr;
}

static void RemoveTimers(GarrysMod::Lua::ILuaInterface* pLua)
{
	auto pData = GetSysTimerLuaData(pLua);

	bool bDeleted = false;
	int nTimer = pData->pLuaTimers.Count();
	for (int i = nTimer - 1; i >= 0; i--)
	{
		ILuaTimer* timer = pData->pLuaTimers[i];
		if (!timer->markDelete)
			continue;

		pData->pLuaTimers.FastRemove(nTimer);
		bDeleted = true;
		delete timer;
	}

	if (bDeleted)
		pData->pLuaTimers.Compact();
}

LUA_FUNCTION_STATIC(timer_Adjust)
{
	const char* name = LUA->CheckString(1);
	double delay = LUA->CheckNumber(2) * 1000 * 1000;

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer) {
		timer->delay = (float)delay;
		if (LUA->IsType(3, GarrysMod::Lua::Type::Number))
			timer->repetitions = (int)LUA->GetNumber(3);

		if (LUA->IsType(4, GarrysMod::Lua::Type::Function)) {
			Util::ReferenceFree(LUA, timer->function, "timer.Adjust - old function");
			LUA->Push(4);
			timer->function = Util::ReferenceCreate(LUA, "timer.Adjust - new function");
		}

		LUA->PushBool(true);
	} else
		LUA->PushBool(false);

	return 1;
}

LUA_FUNCTION_STATIC(timer_Check)
{
	return 0;
}

LUA_FUNCTION_STATIC(timer_Create)
{
	const char* name = LUA->CheckString(1);
	double delay = LUA->CheckNumber(2) * 1000 * 1000;
	double repetitions = LUA->CheckNumber(3);
	LUA->CheckType(4, GarrysMod::Lua::Type::Function);

	bool bNewTimer = false;
	ILuaTimer* timer = FindTimer(LUA, name); // Reuse existing timer
	if (!timer)
	{
		timer = new ILuaTimer;
		bNewTimer = true;
	} else {
		Util::ReferenceFree(timer->pLua, timer->function, "timer.Create - old function");
		Util::ReferenceFree(timer->pLua, timer->identifierReference, "timer.Create - old identifyer");
	}

	LUA->Push(4);
	timer->function = Util::ReferenceCreate(LUA, "timer.Create - new function");

	timer->identifier = name;
	LUA->Push(1);
	timer->identifierReference = Util::ReferenceCreate(LUA, "timer.Create - new identifyer");
	timer->delay = (float)delay;
	timer->repetitions = (int)repetitions;
	timer->nextRunTime = GetTime() + delay;
	timer->pLua = LUA;

	if (bNewTimer)
		GetSysTimerLuaData(LUA)->pLuaTimers.AddToTail(timer);

	return 0;
}

LUA_FUNCTION_STATIC(timer_Exists)
{
	const char* name = LUA->CheckString(1);

	LUA->PushBool(FindTimer(LUA, name) != nullptr);

	return 1;
}

LUA_FUNCTION_STATIC(timer_Pause)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer) {
		if (timer->active) {
			timer->active = false;
			LUA->PushBool(true);
		} else
			LUA->PushBool(false);
	} else
		LUA->PushBool(false);

	return 1;
}

LUA_FUNCTION_STATIC(timer_Remove)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer) {
		timer->markDelete = true;
		RemoveTimers(LUA);
	}

	return 0;
}

LUA_FUNCTION_STATIC(timer_RepsLeft)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer) {
		if (timer->repetitions > 0)
			LUA->PushNumber(timer->repetitions);
		else
			LUA->PushNumber(0);
	} else
		LUA->PushNumber(0);

	return 1;
}

LUA_FUNCTION_STATIC(timer_Simple)
{
	double delay = LUA->CheckNumber(1) * 1000 * 1000;
	LUA->CheckType(2, GarrysMod::Lua::Type::Function);

	ILuaTimer* timer = new ILuaTimer;
	LUA->Push(2);
	timer->function = Util::ReferenceCreate(LUA, "timer.Simple - function");

	timer->delay = (float)delay;
	timer->repetitions = 1;
	timer->nextRunTime = GetTime() + delay;
	timer->pLua = LUA;

	GetSysTimerLuaData(LUA)->pLuaTimers.AddToTail(timer);

	return 0;
}

LUA_FUNCTION_STATIC(timer_Start)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer) {
		if (!timer->active) {
			timer->active = true;
			timer->nextRunTime = GetTime() + timer->nextRunTime;
			LUA->PushBool(true);
		} else
			LUA->PushBool(false);
	} else
		LUA->PushBool(false);

	return 1;
}

LUA_FUNCTION_STATIC(timer_Stop)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer) {
		if (timer->active) {
			timer->active = false;
			timer->nextRunTime = timer->nextRunTime - GetTime(); // Do we care if it becomes possibly negative?
			LUA->PushBool(true);
		} else
			LUA->PushBool(false);
	} else
		LUA->PushBool(false);

	return 1;
}

LUA_FUNCTION_STATIC(timer_TimeLeft)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer)
		if (timer->active)
			LUA->PushNumber(timer->nextRunTime - GetTime());
		else
			LUA->PushNumber(timer->nextRunTime);
	else
		LUA->PushNumber(0);

	return 1;
}

LUA_FUNCTION_STATIC(timer_Toggle)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer) {
		timer->active = !timer->active;
		timer->nextRunTime = GetTime() + timer->nextRunTime;
		LUA->PushBool(timer->active);
	} else
		LUA->PushBool(false);

	return 1;
}

LUA_FUNCTION_STATIC(timer_UnPause)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(LUA, name);
	if (timer) {
		timer->active = true;
		timer->nextRunTime = GetTime() + timer->delay;
		LUA->PushBool(true);
	} else
		LUA->PushBool(false);

	return 1;
}

void CSysTimerModule::LuaInit(GarrysMod::Lua::ILuaInterface* pLua, bool bServerInit)
{
	if (bServerInit)
		return;

	Lua::GetLuaData(pLua)->SetModuleData(m_pID, new LuaSysTimerModuleData);

	Util::StartTable(pLua);
		Util::AddFunc(pLua, timer_Adjust, "Adjust");
		Util::AddFunc(pLua, timer_Check, "Check");
		Util::AddFunc(pLua, timer_Create, "Create");
		Util::AddFunc(pLua, timer_Remove, "Destroy");
		Util::AddFunc(pLua, timer_Exists, "Exists");
		Util::AddFunc(pLua, timer_Pause, "Pause");
		Util::AddFunc(pLua, timer_Remove, "Remove");
		Util::AddFunc(pLua, timer_RepsLeft, "RepsLeft");
		Util::AddFunc(pLua, timer_Simple, "Simple");
		Util::AddFunc(pLua, timer_Start, "Start");
		Util::AddFunc(pLua, timer_Stop, "Stop");
		Util::AddFunc(pLua, timer_TimeLeft, "TimeLeft");
		Util::AddFunc(pLua, timer_Toggle, "Toggle");
		Util::AddFunc(pLua, timer_UnPause, "UnPause");
	Util::FinishTable(pLua, "systimer");
}

void CSysTimerModule::LuaShutdown(GarrysMod::Lua::ILuaInterface* pLua)
{
	auto pData = GetSysTimerLuaData(pLua);

	FOR_EACH_VEC(pData->pLuaTimers, i)
		delete pData->pLuaTimers[i];

	pData->pLuaTimers.RemoveAll(),
	Util::NukeTable(pLua, "systimer");
}

void CSysTimerModule::LuaThink(GarrysMod::Lua::ILuaInterface* pLua)
{
	VPROF_BUDGET("HolyLib - CSysTimerModule::LuaThink", VPROF_BUDGETGROUP_HOLYLIB);

	double time = GetTime();
	auto pData = GetSysTimerLuaData(pLua);
	for (ILuaTimer* timer : pData->pLuaTimers)
	{
		if (!timer->active)
			continue;

		if (g_pSysTimerModule.InDebug())
			Msg("Time: %f\nNext: %f\nRun Time: %f\n", time, timer->nextRunTime - GetTime(), timer->nextRunTime);
		
		if ((timer->nextRunTime - GetTime()) <= 0)
		{
			timer->nextRunTime = time + timer->delay;

			Util::ReferencePush(pLua, timer->function);
			pLua->CallFunctionProtected(0, 0, true); // We should add a custom error handler to not have errors with no stack (Which somehow can happen but only observed in gmod clients)

			if (timer->repetitions == 1)
				timer->markDelete = true;
			else
				timer->repetitions--;
		}
	}

	RemoveTimers(pLua);
}