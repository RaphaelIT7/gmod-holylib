#include "detours.h"
#include <map>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

unsigned int g_pCurrentCategory = 0;

SymbolFinder Detour::symfinder;
void* Detour::GetFunction(void* module, Symbol symbol)
{
	return symfinder.Resolve(module, symbol.name.c_str(), symbol.length);
}

std::map<unsigned int, std::vector<Detouring::Hook*>> g_pDetours = {};
void Detour::Create(Detouring::Hook* hook, const char* name, void* module, Symbol symbol, void* hook_func, unsigned int category)
{
	void* func = Detour::GetFunction(module, symbol);
	if (!CheckFunction(func, name))
		return;

	hook->Create(func, hook_func);
	hook->Enable();

	g_pDetours[category].push_back(hook);

	if (!DETOUR_ISVALID((*hook))) {
		Msg("Failed to detour %s!\n", name);
	}
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
	for (auto& [id, hooks]: g_pDetours) {
		if (hooks.size() > 0)
		{
			Msg("ID: %d failed to shutdown it's detours!\n", id);
		}
	}
}