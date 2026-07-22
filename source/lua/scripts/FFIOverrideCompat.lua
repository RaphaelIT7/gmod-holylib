if not rawget(_G, "__HOLYLIB_ENABLE_FFI_OVERRIDES") then
    return
end

local typeID = TypeID
local vectorTypeID = TYPE_VECTOR or 10
local angleTypeID = TYPE_ANGLE or 11
local wrapInclude = rawget(_G, "__HOLYLIB_FFI_COMPAT_WRAP_INCLUDE") ~= false

if type(typeID) ~= "function" then
    error("HolyLib FFI overrides require TypeID")
end

local function installTypeChecks()
    local function isvector(value)
        return typeID(value) == vectorTypeID
    end

    local function isangle(value)
        return typeID(value) == angleTypeID
    end

    debug.setblocked(isvector)
    debug.setblocked(isangle)

    -- Stock includes/util.lua replaces all three while Lua is still booting.
    _G.TypeID = typeID
    _G.isvector = isvector
    _G.isangle = isangle
end

installTypeChecks()

local originalInclude = include
if wrapInclude and type(originalInclude) == "function" then
    local function isUtilInclude(path)
        if type(path) ~= "string" then
            return false
        end

        local normalizedPath = string.lower(string.gsub(path, "\\", "/"))
        return normalizedPath == "util.lua" or string.sub(normalizedPath, -17) == "includes/util.lua"
    end

    function include(path, ...)
        if isUtilInclude(path) then
            -- util.lua replaces the checks. Restore stock include before its call and
            -- reinstall on the next tick, after the synchronous include has returned.
            -- The tail call below keeps this wrapper out of include error attribution.
            _G.include = originalInclude
            timer.Simple(0, installTypeChecks)
        end

        return originalInclude(path, ...)
    end

    debug.setblocked(include)
end
