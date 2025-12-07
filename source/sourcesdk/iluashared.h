#ifndef GMOD_ILUASHARED_H
#define GMOD_ILUASHARED_H
#include <string>
#include <vector>
#include "Bootil/Bootil.h"
#include "Bootil/Types/Buffer.h"
#include "interface.h"

namespace GarrysMod::Lua
{
	namespace State
	{
		enum
		{
			CLIENT = 0,
			SERVER,
			MENU
		};

		static const char *Name[] = {
			"client",
			"server",
			"menu",
			nullptr
		};
	}

	struct LuaFile
	{
		~LuaFile();
		int time;
	#ifdef WIN32
		std::string name;
		std::string source;
		std::string contents;
		inline const char* GetName() { return name.c_str(); }
		inline const char* GetSource() { return source.c_str(); }
		inline const char* GetContents() { return contents.c_str(); }
	#else
		const char* name;
		const char* source;
		const char* contents;
		inline const char* GetName() { return name; }
		inline const char* GetSource() { return source; }
		inline const char* GetContents() { return contents; }
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
	#ifdef WIN32
		std::string fileName;
		inline const char* GetFileName() { return fileName.c_str(); }
	#else
		const char* fileName;
		inline const char* GetFileName() { return fileName; }
	#endif
		bool isFolder;
	};
}

class LuaClientDatatableHook;
class CSteamAPIContext;
class IGet;

#define GMOD_LUASHARED_INTERFACE "LUASHARED003"
namespace GarrysMod::Lua
{
	class ILuaInterface;
	class ILuaShared
	{
	public:
		virtual ~ILuaShared() {};                                                                                                        // x86: 0x00, x64: 0x00
		virtual void Init(CreateInterfaceFn, bool, CSteamAPIContext*, IGet*) = 0;                                                       // x86: 0x08, x64: 0x10
		virtual void Shutdown() = 0;                                                                                                     // x86: 0x0C, x64: 0x18
		virtual void DumpStats() = 0;                                                                                                    // x86: 0x10, x64: 0x20
		virtual ILuaInterface* CreateLuaInterface(unsigned char, bool) = 0;                                                             // x86: 0x14, x64: 0x28
		virtual void CloseLuaInterface(ILuaInterface*) = 0;                                                                             // x86: 0x18, x64: 0x30
		virtual ILuaInterface* GetLuaInterface(unsigned char) = 0;                                                                      // x86: 0x1C, x64: 0x38
		virtual GarrysMod::Lua::LuaFile* LoadFile(const std::string& path, const std::string& pathId, bool fromDatatable, bool fromFile) = 0; // x86: 0x20, x64: 0x40
		virtual GarrysMod::Lua::LuaFile* GetCache(const std::string&) = 0;                                                             // x86: 0x24, x64: 0x48
		virtual void MountLua(const char*) = 0;                                                                                         // x86: 0x28, x64: 0x50
		virtual void MountLuaAdd(const char*, const char*) = 0;                                                                         // x86: 0x2C, x64: 0x58
		virtual void UnMountLua(const char*) = 0;                                                                                       // x86: 0x30, x64: 0x60
		virtual void SetFileContents(const char*, const char*) = 0;                                                                     // x86: 0x34, x64: 0x68
		virtual void SetLuaFindHook(LuaClientDatatableHook*) = 0;                                                                       // x86: 0x38, x64: 0x70
		virtual void __UnsafeFindScripts(const std::string&, const std::string&, std::vector<GarrysMod::Lua::LuaFindResult>&) = 0;      // x86: 0x3C, x64: 0x78
		virtual const char* GetStackTraces() = 0;                                                                                       // x86: 0x40, x64: 0x80
		virtual void InvalidateCache(const std::string&) = 0;                                                                           // x86: 0x44, x64: 0x88
		virtual void EmptyCache() = 0;                                                                                                  // x86: 0x48, x64: 0x90
		virtual bool ScriptExists(const std::string&, const std::string&, bool) = 0;                                                   // x86: 0x4C, x64: 0x98
		
	public:
		// Helper function to safely call FindScripts that works on both x64 and x86
		// In x64, this calls through the vtable with __fastcall to fix ABI issues
		inline void FindScripts(const std::string& pattern, const std::string& pathId, std::vector<GarrysMod::Lua::LuaFindResult>& results) {
	#if SYSTEM_WINDOWS
			// In x64, the function uses __fastcall and references are passed as pointers
			// Signature from IDA: __int64 __fastcall CLuaShared::FindScripts(__int64, _QWORD *, _QWORD *, __int64)
			// FindScripts is at offset 0x78 in x64 vtable (index 15)
			void** vtable = *(void***)(this);
			// __fastcall: this in RCX, then params in RDX, R8, R9
			// References are passed as pointers in x64
			using FindScriptsFunc = void(__fastcall*)(void* thisptr, const std::string*, const std::string*, std::vector<LuaFindResult>*);
			FindScriptsFunc pFindScripts = reinterpret_cast<FindScriptsFunc>(vtable[15]);
			pFindScripts(this, &pattern, &pathId, &results);
	#else
			// x86: use normal virtual call
			__UnsafeFindScripts(pattern, pathId, results);
	#endif
		}
	};
}
#endif