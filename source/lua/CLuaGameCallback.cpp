#include "LuaInterface.h"
#include <GarrysMod/Lua/LuaGameCallback.h>
#include "color.h"
#include <sstream>
#include <string>
#include "Platform.hpp"

class CLuaGameCallback : public GarrysMod::Lua::ILuaGameCallback
{
public:
	virtual GarrysMod::Lua::ILuaObject* CreateLuaObject();
	virtual void DestroyLuaObject(GarrysMod::Lua::ILuaObject* pObject);
	virtual void ErrorPrint(const char* error, bool print);
	virtual void Msg(const char* msg, bool unknown);
	virtual void MsgColour(const char* msg, const Color& color);
	virtual void LuaError(const CLuaError* error);
	virtual void InterfaceCreated(GarrysMod::Lua::ILuaInterface* iface);
};

#ifdef CLIENT_DLL
Color col_msg(255, 241, 122, 200);
Color col_error(255, 221, 102, 255);
#elif defined(MENUSYSTEM)
Color col_msg(100, 220, 100, 200);
Color col_error(120, 220, 100, 255);
#else
Color col_msg(156, 241, 255, 200);
Color col_error(136, 221, 255, 255);
#endif

void UTLVarArgs(char* buffer, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
}

GarrysMod::Lua::ILuaObject* CLuaGameCallback::CreateLuaObject()
{
	Error("CLuaGameCallback::CreateLuaObject was called! (Should never be used)\n");
	return NULL;
}

void CLuaGameCallback::DestroyLuaObject(GarrysMod::Lua::ILuaObject* pObject)
{
	Error("CLuaGameCallback::DestroyLuaObject was called! (Should never be used)\n");
}

void CLuaGameCallback::ErrorPrint(const char* error, bool print)
{
	// Write into the lua_errors_server.txt if error logging is enabled.

	Color ErrorColor = col_error; // ToDo: Change this later and finish this function.
	ConColorMsg(ErrorColor, "%s\n", error);
}

void CLuaGameCallback::Msg(const char* msg, bool unknown)
{
	MsgColour(msg, col_msg);
}

void CLuaGameCallback::MsgColour(const char* msg, const Color& color)
{
	ConColorMsg(color, "%s", msg);
}

void CLuaGameCallback::LuaError(const CLuaError* error)
{
	std::stringstream str;

	str << "[ERROR] ";
	str << error->message;
	str << "\n";

	int i = 0;
	for (CLuaError::StackEntry entry : error->stack)
	{
		++i;
		for (int j=-1;j<i;++j)
		{
			str << " ";
		}

		str << i;
		str << ". ";
		str << entry.function;
		str << " - ";
		str << entry.source;
		str << ":";
		str << entry.line;
		str << "\n";
	}

	MsgColour(str.str().c_str(), col_error);
}

void CLuaGameCallback::InterfaceCreated(GarrysMod::Lua::ILuaInterface* iface) {} // Unused

static CLuaGameCallback gamecallback;
GarrysMod::Lua::ILuaGameCallback* g_LuaCallback = &gamecallback;