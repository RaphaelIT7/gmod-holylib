--[[
    This has the simple purpose of creating HolyLib's UserData and creating the CType that is then later used to allocate data pushed from C to Lua as cdata when possible
]]

if true then return end -- Disabled since it needs a major rework first. We recently changed the ENTIRE userdata setup.

local metaTableCache = {}
__HOLYLIB_FFI = __HOLYLIB_FFI or {}
function __HOLYLIB_FFI.RegisterOverride(metaTable)
    print("HolyLib-FFI: Registered FFI override for " .. metaTable.MetaName)
    metaTableCache[metaTable.MetaID] = metaTable
end

local mt = {
    __index = function(self, key)
        local metaID = self.type
        local metaTable = metaTableCache[metaID]
        if not metaTable then
            metaTableCache[metaID] = jit.getMetaTableByID(metaID)

            if not metaTable then
                print("Failing to fetch MetaTable for id \"" .. metaID .. "\"")
                return
            end
        end

        if metaTable.__index then
            return metaTable.__index(self, key)
        end
    end,
    __newindex = function(self, key, value)
        local metaID = self.type
        local metaTable = metaTableCache[metaID]
        if not metaTable then
            metaTableCache[metaID] = jit.getMetaTableByID(metaID)

            if not metaTable then
                print("Failing to fetch MetaTable for id \"" .. metaID .. "\"")
                return
            end
        end

        if metaTable.__newindex then
            return metaTable.__newindex(self, key, value)
        end
    end,
    __gc = function(self)
        local metaID = self.type
        local metaTable = metaTableCache[metaID]
        if not metaTable then
            metaTableCache[metaID] = jit.getMetaTableByID(metaID)

            if not metaTable then
                print("Failing to fetch MetaTable for id \"" .. metaID .. "\"")
                return
            end
        end

        if metaTable.__gc then
            return metaTable.__gc(self)
        end
    end
}

do
    local ffi = jit.getffi and jit.getffi() or require("ffi")

    ffi.cdef([[
        typedef struct {
            const uintptr_t data;
            const unsigned char type;
            const unsigned char additionalData;
            const unsigned char _offset1;
            const unsigned char _offset2;
            const unsigned short _offset3;
            const int m_iReference;
            const int m_iTableReference;
        } HOLYLIB_UserData;
    ]])

    jit.registerCreateHolyLibCDataFunc("HOLYLIB_UserData", mt, mt.__gc)
end