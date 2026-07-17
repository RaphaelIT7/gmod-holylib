local typeID = TypeID
local vectorTypeID = TYPE_VECTOR or 10
local angleTypeID = TYPE_ANGLE or 11

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
if type(originalInclude) == "function" then
    local function pack(...)
        return { n = select("#", ...), ... }
    end

    function include(path, ...)
        local results = pack(originalInclude(path, ...))

        if type(path) == "string" then
            local normalizedPath = string.lower(string.gsub(path, "\\", "/"))
            if normalizedPath == "util.lua" or string.sub(normalizedPath, -17) == "includes/util.lua" then
                installTypeChecks()
            end
        end

        return unpack(results, 1, results.n)
    end

    debug.setblocked(include)
end
