#include "detours.h"
#include <map>
#include <convar.h>
#include <unordered_set>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef PROJECT_NAME // When people copy this file they might not have it defined, so we just fallback.
#define PROJECT_NAME "project"
#endif

unsigned int g_pCurrentCategory = 0;

SymbolFinder Detour::symfinder;
void* Detour::GetFunction(void* pModule, Symbol pSymbol)
{
	return symfinder.Resolve(pModule, pSymbol.name.c_str(), pSymbol.length);
}

std::unordered_set<std::string> pDisabledDetours;
std::map<unsigned int, std::unordered_set<Detouring::Hook*>> g_pDetours = {};
void Detour::Create(Detouring::Hook* pHook, const char* strName, void* pModule, Symbol pSymbol, void* pHookFunc, unsigned int category)
{
	if (pDisabledDetours.find(strName) != pDisabledDetours.end())
	{
		Msg(PROJECT_NAME ": Detour %s was disabled!\n", strName);
		return;
	}

	void* func = Detour::GetFunction(pModule, pSymbol);
	if (!CheckFunction(func, strName))
		return;

	pHook->Create(func, pHookFunc);
	pHook->Enable();

	if (g_pDetours[category].find(pHook) == g_pDetours[category].end())
	{
		g_pDetours[category].insert(pHook);
	}

	if (!DETOUR_ISVALID((*pHook)))
		Msg(PROJECT_NAME ": Failed to detour %s!\n", strName);
}

void Detour::Remove(unsigned int category) // NOTE: Do we need to check if the provided category is valid?
{
	for (Detouring::Hook* hook : g_pDetours[category]) {
		if (hook->IsEnabled())
		{
			hook->Disable();
			hook->Destroy();
		}
	}
	g_pDetours[category].clear();
}

void Detour::ReportLeak()
{
	for (auto& [id, hooks]: g_pDetours)
		if (hooks.size() > 0)
			Msg(PROJECT_NAME ": ID %d failed to shutdown it's detours!\n", id);
}

static void ToggleDetour(const CCommand& args)
{
	if (args.ArgC() < 1 || V_stricmp(args.Arg(1), "") == 0)
	{
		Msg("Disabled Detours:\n");
		for (auto it = pDisabledDetours.begin(); it != pDisabledDetours.end(); ++it)
			Msg("	- \"%s\"\n", it->c_str());

		return;
	}
	
	auto it = pDisabledDetours.find(args.Arg(1));
	if (it != pDisabledDetours.end())
	{
		pDisabledDetours.erase(it);
		Msg("Enabled detour %s!\n", args.Arg(1));
		return;
	}

	pDisabledDetours.insert(args.Arg(1));
	Msg("Disabled detour %s!\n", args.Arg(1));
}
static ConCommand toggledetour("holylib_toggledetour", ToggleDetour, "Debug command. Enables/Disables the given detour.", 0);