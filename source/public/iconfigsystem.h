#pragma once

#include "interface.h"
#include "Bootil/Bootil.h"

#ifdef PLATFORM_64BITS
#define HOLYLIB_CONFIG_PATH "garrysmod/holylib/cfg/x64/"
#else
#define HOLYLIB_CONFIG_PATH "garrysmod/holylib/cfg/x86/"
#endif

/*
	This is the exposed config system used by holylib for it's config files.
*/

enum ConfigState
{
	NOT_LOADED = -2, // The config wasn't loaded yet ._.
	INVALID_JSON = -1, // It failed to load the json 
	OK = 0, // It's fine
	FRESH = 1, // It's a fresh config
};

abstract_class IConfig
{
public:
	virtual ~IConfig() {};

	// Returns true if the config was just created. Add your default values and then save it!
	virtual ConfigState GetState() = 0;

	// Config data.
	virtual Bootil::Data::Tree& GetData() = 0;

	// Saves the config
	virtual bool Save() = 0;

	// Loads the config from disk. Returns false if any issues occurred on loading.
	virtual bool Load() = 0;

	// destroys the config invalidating it without saving!
	virtual void Destroy() = 0;
};

abstract_class IConfigSystem
{
public:
	/*
		Attempts to load the config from the given file path
		The path needs to be relative to the base path of the game.
		Currently this will NEVER return NULL.
	*/
	virtual IConfig* LoadConfig(const char* pFilePath) = 0;
};

extern IConfigSystem* g_pConfigSystem;

#define INTERFACEVERSION_CONFIGSYSTEM "ICONFIGSYSTEM001"