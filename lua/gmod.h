#include "lj_obj.h"

#ifdef __cplusplus
extern "C" {
#endif
extern void lua_init_stack_gmod(lua_State* L1, lua_State* L);
extern void GMOD_LuaPrint(const char* str, lua_State* L);
extern void GMOD_LuaCreateEmptyUserdata(lua_State* L);

// Was moved into the lj_api.c file since the linker hated me.
//extern const char* GMODLUA_GetUserType(lua_State* L, int iType);
#ifdef __cplusplus
}
#endif