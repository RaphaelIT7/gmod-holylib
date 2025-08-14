-- Cons of this AngleFFI:
-- setting x/y/z to a string that contains a number, will error, eg. v.x = "1" won't work but works in gmod
-- cannot grab the metatable of the angtor, nor edit it, so you cannot add custom methods to the angtor

-- Collaboration between Raphael & Srlion (https://github.com/Srlion) <3

local CreateAngle, isangle

do
    local old_type = type
    -- Override the type function to return "Angle" for our custom angtor type
    function type(v)
        if isangle(v) then
            return "Angle"
        end
        return old_type(v)
    end
end

-- We cannot localize type since for the AngleFFI we override it!
-- local type = type
local tonumber = tonumber

local function Angle(x, y, z)
    -- Angle() in gmod, doesn't error for invalid arguments
    local ang = x
    if isangle(ang) then
        return CreateAngle(ang.x, ang.y, ang.z)
    end
    return CreateAngle(tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0)
end
_G.GMOD_Angle = _G.Angle -- let's keep the original around
_G.Angle = Angle

---@param value any
---@param expected_type string
---@param arg_num number
---@param is_optional boolean|nil
---@return any
local function expect(value, expected_type, arg_num, is_optional)
    -- If marked as optional and value is nil, return nil
    if is_optional and value == nil then
        return nil
    end
    local actual_type = type(value)
    if actual_type ~= expected_type then
        local caller = debug.getinfo(2, "n").name or "unknown"
        local type_str = is_optional and (expected_type .. " or nil") or expected_type
        return error(string.format("bad argument #%d to '%s' (%s expected, got %s)",
            arg_num,
            caller,
            type_str,
            actual_type), 2)
    end
    return value
end

local function check_num(value, arg_num, is_optional)
    return expect(value, "number", arg_num, is_optional)
end

local function check_ang(value, arg_num, is_optional)
    return expect(value, "Angle", arg_num, is_optional)
end

local function check_ang_or_num(value, arg_num, is_optional)
    local t = type(value)
    if t == "Angle" then
        -- we use Unpack, because if gmod's Angle is passed, it will be faster to use Unpack instead of accessing x, y, z directly
        return value:Unpack()
    elseif t == "number" then
        return value, value, value
    end
    return expect(value, "number", arg_num, is_optional)
end

local methods = {}
local mt = {
    __index = function(s, k)
        local method = methods[k]
        if method then
            return method
        end

        if k == 1 or k == "p" then
            return s.x
        elseif k == 2 or k == "y" then
            return s.y
        elseif k == 3 or k == "r" then
            return s.z
        end
    end,
    __newindex = function(s, k, v)
        local num = check_num(v, 3)
        if k == 1 or k == "p" or k == "pitch" or k == "x" then
            s.x = num
        elseif k == 2 or k == "y" or k == "yaw" then
            s.y = num
        elseif k == 3 or k == "r" or k == "roll" or k == "z" then
            s.z = num
        else
            -- Normal Gmod Angle's do nothing in this case.
            -- error(string.format("cannot set field '%s' on FFI Angle", k), 2)
        end
    end,
    __eq = function(a, b)
        if type(b) ~= "Angle" then
            return false
        end
        return a.x == b.x and a.y == b.y and a.z == b.z
    end,
    __add = function(a, b)
        check_ang(b, 2)
        return Angle(a.x + b.x, a.y + b.y, a.z + b.z)
    end,
    __sub = function(a, b)
        check_ang(b, 2)
        return Angle(a.x - b.x, a.y - b.y, a.z - b.z)
    end,
    __mul = function(a, b)
        local x, y, z = check_ang_or_num(b, 2)
        return Angle(a.x * x, a.y * y, a.z * z)
    end,
    __div = function(a, b)
        local x, y, z = check_ang_or_num(b, 2)
        return Angle(a.x / x, a.y / y, a.z / z)
    end,
    __unm = function(a)
        return Angle(-a.x, -a.y, -a.z)
    end,
    __tostring = function(a)
        return string.format("%f %f %f", a.x, a.y, a.z)
    end,
    MetaName = "Angle",
    MetaID = 11,
}

-- We do this so that we also have things like Angle():__tostring() working
for name, func in pairs(mt) do
    if isfunction(func) then
        methods[name] = func
    end
end

function methods:Add(v)
    check_ang(v, 1)

    self.x = self.x + v.x
    self.y = self.y + v.y
    self.z = self.z + v.z
end

function methods:Sub(v)
    check_ang(v, 1)

    self.x = self.x - v.x
    self.y = self.y - v.y
    self.z = self.z - v.z
end

function methods:Div(div)
    local x, y, z = check_ang_or_num(div, 1)

    self.x = self.x / x
    self.y = self.y / y
    self.z = self.z / z
end

function methods:Mul(multiplier)
    local x, y, z = check_ang_or_num(multiplier, 1)

    self.x = self.x * x
    self.y = self.y * y
    self.z = self.z * z
end

function methods:IsEqualTol(compare, tolerance)
    return math.abs(self.x - compare.x) <= tolerance and math.abs(self.y - compare.y) <= tolerance and
        math.abs(self.z - compare.z) <= tolerance
end

function methods:IsZero()
    return self.x == 0 and self.y == 0 and self.z == 0
end

function methods:Normalize()
    local length = math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
    if length == 0 then return end

    self.x = self.x / length
    self.y = self.y / length
    self.z = self.z / length
end

function methods:Random(min, max)
    min = min or -1
    max = max or -1

    self.x = math.random(min, max)
    self.y = math.random(min, max)
    self.z = math.random(min, max)
end

function methods:Set(v)
    check_ang(v, 1)

    self.x = v.x
    self.y = v.y
    self.z = v.z
end

function methods:SetUnpacked(x, y, z)
    check_num(x, 1)
    check_num(y, 2)
    check_num(z, 3)

    self.x = x
    self.y = y
    self.z = z
end

function methods:Unpack()
    return self.x, self.y, self.z
end

function methods:Zero()
    self.x = 0
    self.y = 0
    self.z = 0
end

function methods:ToTable()
    return { self.x, self.y, self.z }
end

function methods:Up()
    local pr = math.rad(self.p)
    local cp = math.cos(pr)
    local sp = math.sin(pr)
    local yr = math.rad(self.y)
    local cy = math.cos(yr)
    local sy = math.sin(yr)
    local rr = math.rad(self.r)
    local cr = math.cos(rr)
    local sr = math.sin(rr)

    local x = -sr * sp * cy + cr * sy
    local y = -sr * sp * sy - cr * cy
    local z = -sr * cp

    return Vector(x, y, z)
end

function methods:Forward()
    local pr = math.rad(self.p)
    local cp = math.cos(pr)
    local sp = math.sin(pr)
    local yr = math.rad(self.y)
    local cy = math.cos(yr)
    local sy = math.sin(yr)

    local x = cp * cy
    local y = cp * sy
    local z = -sp

    return Vector(x, y, z)
end

---@class Angle
local gmodAngMeta = FindMetaTable("Angle")
methods.RotateAroundAxis = gmodAngMeta.RotateAroundAxis -- ToDo

do
    local ffi = jit.getffi and jit.getffi() or require("ffi")

    ffi.cdef([[
        typedef struct { const uintptr_t data; const uint8_t type; float x, y, z; } GMOD_AngUserData;
    ]])

    local RawAngle = ffi.metatype("GMOD_AngUserData", mt)
    ---@return Angle
    function CreateAngle(x, y, z)
        local ang = RawAngle(0, mt.MetaID, x, y, z)

        -- Get a pointer to the data field and set it directly
        local ang_ptr = ffi.cast("uintptr_t*", ang)
        ang_ptr[0] = ffi.cast("uintptr_t", ffi.cast("uint8_t*", ang) + ffi.offsetof("GMOD_AngUserData", "x"))

        ---@type Angle
        return ang
    end

    debug.setblocked(CreateAngle)

    function isangle(v)
        return ffi.istype("GMOD_AngUserData", v)
    end

    debug.setblocked(isangle)
    _G.GMOD_isangle = _G.isangle
    _G.isangle = isangle
end

jit.markFFITypeAsGmodUserData(Angle(1, 1, 1))