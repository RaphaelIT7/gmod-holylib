#include "LuaInterface.h"
#include "module.h"
#include "lua.h"
#include <chrono>

class CSysTimerModule : public IModule
{
public:
	virtual void LuaInit(bool bServerInit) OVERRIDE;
	virtual void LuaShutdown() OVERRIDE;
	virtual void Think(bool bSimulating) OVERRIDE;
	virtual const char* Name() { return "systimer"; };
	virtual int Compatibility() { return LINUX32 | LINUX64 | WINDOWS32 | WINDOWS64; };
};

static CSysTimerModule g_pSysTimerModule;
IModule* pSysTimerModule = &g_pSysTimerModule;

struct ILuaTimer
{
	~ILuaTimer()
	{
		if (function > 0)
			g_Lua->ReferenceFree(function);

		if (identifierreference > 0)
			g_Lua->ReferenceFree(identifierreference);
	}

	// Should we try to make this struct smaller?
	float delay = 0;
	double next_run_time = 0;
	unsigned int repetitions = 0;
	int function = -1;

	//We could make a seperate struct for timer simple :^
	int identifierreference = -1;
	const char* identifier = nullptr;
	bool active = true;
	bool markdelete = false;
};

double GetTime()
{
	return (double)std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() / 1000;
}

CUtlVector<ILuaTimer*> g_pLuaTimers;
ILuaTimer* FindTimer(const char* name) // We should pobably use a set or so to have faster look up. But my precious memory :(
{
	FOR_EACH_VEC(g_pLuaTimers, i)
	{
		ILuaTimer* timer = g_pLuaTimers[i];
		if (timer->identifier != nullptr && strcmp(name, timer->identifier) == 0)
		{
			return timer;
		}
	}

	return nullptr;
}

void RemoveTimers()
{
	bool bDeleted = false;
	FOR_EACH_VEC(g_pLuaTimers, i)
	{
		ILuaTimer* timer = g_pLuaTimers[i];
		if (!timer->markdelete)
			continue;

		g_pLuaTimers.FastRemove(i); // Is this safe?
		bDeleted = true;
		delete timer;
	}

	if (bDeleted)
		g_pLuaTimers.Compact();
}

LUA_FUNCTION_STATIC(timer_Adjust)
{
	const char* name = LUA->CheckString(1);
	double delay = LUA->CheckNumber(2) * 1000 * 1000;

	ILuaTimer* timer = FindTimer(name);
	if (timer) {
		timer->delay = (float)delay;
		if (LUA->IsType(3, GarrysMod::Lua::Type::Number))
			timer->repetitions = (int)LUA->GetNumber(3);

		if (LUA->IsType(4, GarrysMod::Lua::Type::Function)) {
			LUA->ReferenceFree(timer->function);
			LUA->Push(4);
			timer->function = LUA->ReferenceCreate();
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
	ILuaTimer* timer = FindTimer(name); // Reuse existing timer
	if (!timer)
	{
		timer = new ILuaTimer;
		bNewTimer = true;
	} else {
		LUA->ReferenceFree(timer->function);
		LUA->ReferenceFree(timer->identifierreference);
	}

	LUA->Push(4);
	timer->function = LUA->ReferenceCreate();

	timer->identifier = name;
	LUA->Push(1);
	timer->identifierreference = LUA->ReferenceCreate();
	timer->delay = (float)delay;
	timer->repetitions = (int)repetitions;
	timer->next_run_time = GetTime() + delay;

	if (bNewTimer)
		g_pLuaTimers.AddToTail(timer);

	return 0;
}

LUA_FUNCTION_STATIC(timer_Exists)
{
	const char* name = LUA->CheckString(1);

	LUA->PushBool(FindTimer(name) != nullptr);

	return 1;
}

LUA_FUNCTION_STATIC(timer_Pause)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(name);
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

	ILuaTimer* timer = FindTimer(name);
	if (timer) {
		timer->markdelete = true;
		RemoveTimers();
	}

	return 0;
}

LUA_FUNCTION_STATIC(timer_RepsLeft)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(name);
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
	timer->function = LUA->ReferenceCreate();

	timer->delay = (float)delay;
	timer->repetitions = 1;
	timer->next_run_time = GetTime() + delay;
	g_pLuaTimers.AddToTail(timer);

	return 0;
}

LUA_FUNCTION_STATIC(timer_Start)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(name);
	if (timer) {
		if (!timer->active) {
			timer->active = true;
			timer->next_run_time = GetTime() + timer->next_run_time;
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

	ILuaTimer* timer = FindTimer(name);
	if (timer) {
		if (timer->active) {
			timer->active = false;
			timer->next_run_time = timer->next_run_time - GetTime(); // Do we care if it becomes possibly negative?
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

	ILuaTimer* timer = FindTimer(name);
	if (timer)
		if (timer->active)
			LUA->PushNumber(timer->next_run_time - GetTime());
		else
			LUA->PushNumber(timer->next_run_time);
	else
		LUA->PushNumber(0);

	return 1;
}

LUA_FUNCTION_STATIC(timer_Toggle)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(name);
	if (timer) {
		timer->active = !timer->active;
		timer->next_run_time = GetTime() + timer->next_run_time;
		LUA->PushBool(timer->active);
	} else
		LUA->PushBool(false);

	return 1;
}

LUA_FUNCTION_STATIC(timer_UnPause)
{
	const char* name = LUA->CheckString(1);

	ILuaTimer* timer = FindTimer(name);
	if (timer) {
		timer->active = true;
		timer->next_run_time = GetTime() + timer->delay;
		LUA->PushBool(true);
	} else
		LUA->PushBool(false);

	return 1;
}

void CSysTimerModule::LuaInit(bool bServerInit)
{
	if (bServerInit)
		return;

	Util::StartTable();
		Util::AddFunc(timer_Adjust, "Adjust");
		Util::AddFunc(timer_Check, "Check");
		Util::AddFunc(timer_Create, "Create");
		Util::AddFunc(timer_Remove, "Destroy");
		Util::AddFunc(timer_Exists, "Exists");
		Util::AddFunc(timer_Pause, "Pause");
		Util::AddFunc(timer_Remove, "Remove");
		Util::AddFunc(timer_RepsLeft, "RepsLeft");
		Util::AddFunc(timer_Simple, "Simple");
		Util::AddFunc(timer_Start, "Start");
		Util::AddFunc(timer_Stop, "Stop");
		Util::AddFunc(timer_TimeLeft, "TimeLeft");
		Util::AddFunc(timer_Toggle, "Toggle");
		Util::AddFunc(timer_UnPause, "UnPause");
	Util::FinishTable("systimer");
}

void CSysTimerModule::LuaShutdown()
{
	FOR_EACH_VEC(g_pLuaTimers, i)
		delete g_pLuaTimers[i];

	g_pLuaTimers.RemoveAll(),
	Util::NukeTable("systimer");
}

void CSysTimerModule::Think(bool simulating) // Should also be called while hibernating so we should be fine.
{
	VPROF_BUDGET("HolyLib - CSysTimerModule::Think", VPROF_BUDGETGROUP_HOLYLIB);

	double time = GetTime();
	for (ILuaTimer* timer : g_pLuaTimers)
	{
		if (!timer->active)
			continue;

		if (g_pSysTimerModule.InDebug())
			Msg("Time: %f\nNext: %f\nRun Time: %f\n", time, timer->next_run_time - GetTime(), timer->next_run_time);
		
		if ((timer->next_run_time - GetTime()) <= 0)
		{
			timer->next_run_time = time + timer->delay;

			g_Lua->ReferencePush(timer->function);
			g_Lua->CallFunctionProtected(0, 0, true); // We should add a custom error handler to not have errors with no stack (Which somehow can happen but only observed in gmod clients)

			if (timer->repetitions == 1)
				timer->markdelete = true;
			else
				timer->repetitions--;
		}
	}

	RemoveTimers();
}