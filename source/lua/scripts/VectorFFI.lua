-- Cons of this VectorFFI:
-- setting x/y/z to a string that contains a number, will error, eg. v.x = "1" won't work but works in gmod
-- adding data to the vector, like v.hey = 1 will not work, but works in gmod

local POOL_SIZE = 20000

local isvector
do
    local old_type = type
    function type(v)
        if isvector(v) then
            return "Vector"
        end
        return old_type(v)
    end
end

local ffi = jit.getffi and jit.getffi() or require("ffi")
local table = table
local tonumber = tonumber
local type = type

ffi.cdef [[
typedef struct { const uintptr_t data; const uint8_t type; float x, y, z; } GMOD_VecUserData;
]]

function isvector(v)
    return ffi.istype("GMOD_VecUserData", v)
end

debug.setdebugblocked(isvector)

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

local function check_vec(value, arg_num, is_optional, is_optionalType)
    return expect(value, "Vector", arg_num, is_optional)
end

local function check_vec_or_num(value, arg_num, is_optional)
    local t = type(value)
    if t == "Vector" then
        -- we use Unpack, because if gmod's Vector is passed, it will be faster to use Unpack instead of accessing x, y, z directly
        return value:Unpack()
    elseif t == "number" then
        return value, value, value
    end
    return expect(value, "number", arg_num, is_optional)
end

local Vector
local methods = {}
local mt = {
    __index = function(s, k)
        local method = methods[k]
        if method then
            return method
        end

        if k == 1 then
            return s.x
        elseif k == 2 then
            return s.y
        elseif k == 3 then
            return s.z
        end
    end,
    __eq = function(a, b)
        if type(b) ~= "Vector" then
            return false
        end
        return a.x == b.x and a.y == b.y and a.z == b.z
    end,
    __add = function(a, b)
        check_vec(b, 2)
        return Vector(a.x + b.x, a.y + b.y, a.z + b.z)
    end,
    __sub = function(a, b)
        check_vec(b, 2)
        return Vector(a.x - b.x, a.y - b.y, a.z - b.z)
    end,
    __mul = function(a, b)
        local x, y, z = check_vec_or_num(b, 2)
        return Vector(a.x * x, a.y * y, a.z * z)
    end,
    __div = function(a, b)
        local x, y, z = check_vec_or_num(b, 2)
        return Vector(a.x / x, a.y / y, a.z / z)
    end,
    __unm = function(a)
        return Vector(-a.x, -a.y, -a.z)
    end,
    __tostring = function(a)
        return string.format("%f %f %f", a.x, a.y, a.z)
    end,
    MetaName = "Vector",
    MetaID = 10,
}

function methods:Add(v)
    check_vec(v, 1)

    self.x = self.x + v.x
    self.y = self.y + v.y
    self.z = self.z + v.z
end

function methods:Sub(v)
    check_vec(v, 1)

    self.x = self.x - v.x
    self.y = self.y - v.y
    self.z = self.z - v.z
end

function methods:Div(div)
    local x, y, z = check_vec_or_num(div, 1)

    self.x = self.x / x
    self.y = self.y / y
    self.z = self.z / z
end

function methods:Mul(multiplier)
    local x, y, z = check_vec_or_num(multiplier, 1)

    self.x = self.x * x
    self.y = self.y * y
    self.z = self.z * z
end

function methods:Cross(v)
    check_vec(v, 1)

    local cx = self.y * v.z - self.z * v.y
    local cy = self.z * v.x - self.x * v.z
    local cz = self.x * v.y - self.y * v.x

    return Vector(cx, cy, cz)
end

function methods:Distance(v)
    check_vec(v, 1)

    local dx = self.x - v.x
    local dy = self.y - v.y
    local dz = self.z - v.z

    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

function methods:Distance2D(v)
    check_vec(v, 1)

    local dx = self.x - v.x
    local dy = self.y - v.y

    return math.sqrt(dx * dx + dy * dy)
end

function methods:Distance2DSqr(v)
    check_vec(v, 1)

    local dx = self.x - v.x
    local dy = self.y - v.y

    return dx * dx + dy * dy
end

function methods:DistToSqr(v)
    check_vec(v, 1)

    local dx = self.x - v.x
    local dy = self.y - v.y

    return dx * dx + dy * dy
end

function methods:Dot(v)
    check_vec(v, 1)

    return self.x * v.x + self.y * v.y + self.z * v.z
end

methods.DotProduct = methods.Dot

function methods:GetNegated()
    return Vector(-self.x, -self.y, -self.z)
end

function methods:GetNormalized()
    local length = math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
    if length == 0 then
        return 0, 0, 0
    end

    return Vector(self.x / length, self.y / length, self.z / length)
end

methods.GetNormal = methods.GetNormalized

function methods:IsEqualTol(compare, tolerance)
    return math.abs(self.x - compare.x) <= tolerance and math.abs(self.y - compare.y) <= tolerance and
        math.abs(self.z - compare.z) <= tolerance
end

function methods:IsZero()
    return self.x == 0 and self.y == 0 and self.z == 0
end

function methods:Length()
    return math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
end

function methods:Length2D()
    return math.sqrt(self.x * self.x + self.y * self.y)
end

function methods:LengthSqr()
    return self.x * self.x + self.y * self.y + self.z * self.z
end

function methods:Length2DSqr()
    return self.x * self.x + self.y * self.y
end

function methods:Negate()
    self.x = -self.x
    self.y = -self.y
    self.z = -self.z
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
    check_vec(v, 1)

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

---@class Vector
local gmodVecMeta = FindMetaTable("Vector")
methods.Angle = gmodVecMeta.Angle
methods.AngleEx = gmodVecMeta.AngleEx
methods.Rotate = gmodVecMeta.Rotate
methods.ToColor = gmodVecMeta.ToColor
methods.ToScreen = gmodVecMeta.ToScreen
methods.ToTable = gmodVecMeta.ToTable
methods.WithinAABox = gmodVecMeta.WithinAABox

local RawVector = ffi.metatype("GMOD_VecUserData", mt)
---@return Vector
local function CreateVector(x, y, z)
    local vec = RawVector(0, 10, x, y, z)

    -- Get a pointer to the data field and set it directly
    local vec_ptr = ffi.cast("uintptr_t*", vec)
    vec_ptr[0] = ffi.cast("uintptr_t", ffi.cast("uint8_t*", vec) + ffi.offsetof("GMOD_VecUserData", "x"))

    ---@type Vector
    return vec
end
debug.setdebugblocked(CreateVector)

local POOL = {}
local function add_to_pool(v)
    table.insert(POOL, v)
    ffi.gc(v, add_to_pool)
end
debug.setdebugblocked(add_to_pool)

for i = 1, POOL_SIZE do
    local v = CreateVector(0, 0, 0)
    add_to_pool(v)
end

function Vector(x, y, z)
    -- Vector() in gmod, doesn't error for invalid arguments
    x, y, z = tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0
    local v_pool = table.remove(POOL)
    if v_pool then
        v_pool.x, v_pool.y, v_pool.z = x, y, z
        return v_pool
    end
    return CreateVector(x, y, z)
end

jit.markFFITypeAsGmodUserData(Vector(1, 1, 1))

VectorFFI = Vector

print("VectorFFI loaded")