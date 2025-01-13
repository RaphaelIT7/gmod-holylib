#ifndef GMOD_ILUASHARED_H
#define GMOD_ILUASHARED_H
#include <string>
#include <vector>
#include "Bootil/Bootil.h"
#include "Bootil/Types/Buffer.h"
#include "interface.h"

struct LuaFile
{
	~LuaFile();
	int time;
#ifdef WIN32
	std::string name;
	std::string source;
	std::string contents;
#else
	const char* name;
	const char* source;
	const char* contents;
#endif
	Bootil::AutoBuffer compressed;
#ifndef WIN32
	int random = 1; // Unknown thing
#endif
	unsigned int timesloadedserver;
	unsigned int timesloadedclient;
};

struct LuaFindResult
{
	std::string fileName;
	bool isFolder;
};

class LuaClientDatatableHook;
class CSteamAPIContext;

#define GMOD_LUASHARED_INTERFACE "LUASHARED003"
namespace GarrysMod::Lua
{
	class ILuaInterface;
	class ILuaShared
	{
	public:
		virtual ~ILuaShared() {};
		virtual void Init(CreateInterfaceFn, bool, CSteamAPIContext*, IGet*) = 0;
		virtual void Shutdown() = 0;
		virtual void DumpStats() = 0;
		virtual ILuaInterface* CreateLuaInterface(unsigned char, bool) = 0;
		virtual void CloseLuaInterface(ILuaInterface*) = 0;
		virtual ILuaInterface* GetLuaInterface(unsigned char) = 0;
		virtual LuaFile* LoadFile(const std::string& path, const std::string& pathId, bool fromDatatable, bool fromFile) = 0;
		virtual LuaFile* GetCache(const std::string&) = 0;
		virtual void MountLua(const char*) = 0;
		virtual void MountLuaAdd(const char*, const char*) = 0;
		virtual void UnMountLua(const char*) = 0;
		virtual void SetFileContents(const char*, const char*) = 0;
		virtual void SetLuaFindHook(LuaClientDatatableHook*) = 0;
		virtual void FindScripts(const std::string&, const std::string&, std::vector<LuaFindResult>&) = 0;
		virtual const char* GetStackTraces() = 0;
		virtual void InvalidateCache(const std::string&) = 0;
		virtual void EmptyCache() = 0;
		virtual bool ScriptExists(const std::string&, const std::string&, bool) = 0;
	};
}
#endif