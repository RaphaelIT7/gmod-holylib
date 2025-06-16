#pragma once

#include "interface.h"
#include "Bootil/Bootil.h"

/*
	This is the exposed config system used by holylib for it's config files.
*/

abstract_class IConfig
{
public:
	// Returns true if the config was just created. Add your default values and then save it!
	virtual bool FreshConfig() = 0;

	// if this returns true, then the config failed to load because of invalid json!
	virtual bool IsInvalid() = 0;

	// Config data.
	virtual Bootil::Data::Tree& GetData() = 0;

	// Saves the config
	virtual bool Save() = 0;

	// destroys the config invalidating it without saving!
	virtual void Destroy() = 0;
};

abstract_class IConfigSystem
{
public:
	/*
		Attempts to load the config from the given file path
		The path needs to be relative to the base path of the game.
		On failure, this will return NULL.
	*/
	virtual IConfig* LoadConfig(const char* pFilePath) = 0;
};

extern IConfigSystem* g_pConfigSystem;

#define INTERFACEVERSION_CONFIG "ICONFIGSYSTEM001"