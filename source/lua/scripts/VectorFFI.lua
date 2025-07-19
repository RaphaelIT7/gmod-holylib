-- Cons of this VectorFFI:
-- setting x/y/z to a string that contains a number, will error, eg. v.x = "1" won't work but works in gmod
-- cannot grab the metatable of the vector, nor edit it, so you cannot add custom methods to the vector

-- Collaboration between Raphael & Srlion (https://github.com/Srlion) <3

local POOL_SIZE = 20000

local CreateVector, isvector

do
    local old_type = type
    -- Override the type function to return "Vector" for our custom vector type
    function type(v)
        if isvector(v) then
            return "Vector"
        end
        return old_type(v)
    end
end

local type = type
local tonumber = tonumber
local table_remove = table.remove

local POOL = {}

local function Vector(x, y, z)
    -- Vector() in gmod, doesn't error for invalid arguments
    local vec = x
    x, y, z = tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0
    if isvector(vec) then
        x = vec.x
        y = vec.y
        z = vec.z
    end

    local v_pool = table_remove(POOL)
    if v_pool then
        v_pool.x, v_pool.y, v_pool.z = x, y, z
        return v_pool
    end
    return CreateVector(x, y, z)
end
_G.Vector = Vector

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

local function check_ang(value, arg_num, is_optional, is_optionalType)
    return expect(value, "Angle", arg_num, is_optional)
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

local methods = {}
local mt = {
    __index = function(s, k)
        local method = methods[k]
        if method then
            return method
        end

        if k == 1 or k == "x" then
            return s.x
        elseif k == 2 or k == "y" then
            return s.y
        elseif k == 3 or k == "z" then
            return s.z
        end
    end,
    __newindex = function(s, k, v)
        local num = check_num(v, 3)
        if k == 1 or k == "x" then
            s.x = num
        elseif k == 2 or k == "y" then
            s.y = num
        elseif k == 3 or k == "z" then
            s.z = num
        else
            -- Normal Gmod Vector's do nothing in this case.
            -- error(string.format("cannot set field '%s' on FFI Vector", k), 2)
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

-- We do this so that we also have things like Vector():__tostring() working
for name, func in pairs(mt) do
    if isfunction(func) then
        methods[name] = func
    end
end

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
    local dz = self.z - v.z

    return dx * dx + dy * dy + dz * dz
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

function methods:Angle()
    local forward = self:GetNormalized()
    local x = forward.x
    local y = forward.y
    local z = forward.z
    local pitch = 0
    local yaw = 0
    local roll = 0

    yaw = math.deg(math.atan2(y, x))

    local clamped_forward_z = math.min(1, math.max(-1, z))
    pitch = 270 + math.deg(math.acos(clamped_forward_z))

    return Angle(pitch, yaw, roll)
end

function methods:Rotate(rotation) -- This was painful.
    check_ang(rotation, 1)

    local pitch_angle = rotation[1]
    local yaw_angle   = rotation[2]
    local roll_angle  = rotation[3]
    local radPitch    = math.rad(pitch_angle)
    local radYaw      = math.rad(yaw_angle)
    local radRoll     = math.rad(roll_angle)
    local sinPitch    = math.sin(radPitch)
    local cosPitch    = math.cos(radPitch)
    local sinYaw      = math.sin(radYaw)
    local cosYaw      = math.cos(radYaw)
    local sinRoll     = math.sin(radRoll)
    local cosRoll     = math.cos(radRoll)
    local x           = self.x
    local y           = self.y
    local z           = self.z
    local temp_x      = x * cosPitch + z * sinPitch
    local temp_z      = -x * sinPitch + z * cosPitch
    x                 = temp_x
    z                 = temp_z

    temp_x            = x * cosYaw - y * sinYaw
    local temp_y      = x * sinYaw + y * cosYaw
    x                 = temp_x
    y                 = temp_y

    temp_y            = y * cosRoll - z * sinRoll
    temp_z            = y * sinRoll + z * cosRoll
    y                 = temp_y
    z                 = temp_z

    self.x            = x
    self.y            = y
    self.z            = z
end

function methods:ToColor()
    return Color(self.x * 255, self.y * 255, self.z * 255, 255)
end

function methods:ToTable()
    return { self.x, self.y, self.z }
end

function methods:WithinAABox(boxStart, boxEnd)
    check_vec(boxStart, 1)
    check_vec(boxEnd, 2)

    return self.x >= boxStart.x and self.x <= boxEnd.x and
        self.y >= boxStart.y and self.y <= boxEnd.y and
        self.z >= boxStart.z and self.z <= boxEnd.z
end

---@class Vector
local gmodVecMeta = FindMetaTable("Vector")
methods.AngleEx = gmodVecMeta.AngleEx -- Cannot get it to work
methods.ToScreen = gmodVecMeta.ToScreen

do
    local ffi = jit.getffi and jit.getffi() or require("ffi")

    ffi.cdef [[
        typedef struct { const uintptr_t data; const uint8_t type; float x, y, z; } GMOD_VecUserData;
    ]]

    local RawVector = ffi.metatype("GMOD_VecUserData", mt)
    ---@return Vector
    function CreateVector(x, y, z)
        local vec = RawVector(0, 10, x, y, z)

        -- Get a pointer to the data field and set it directly
        local vec_ptr = ffi.cast("uintptr_t*", vec)
        vec_ptr[0] = ffi.cast("uintptr_t", ffi.cast("uint8_t*", vec) + ffi.offsetof("GMOD_VecUserData", "x"))

        ---@type Vector
        return vec
    end

    debug.setblocked(CreateVector)

    function isvector(v)
        return ffi.istype("GMOD_VecUserData", v)
    end

    debug.setblocked(isvector)

    local function initialize_vector_pool()
        local function add_to_pool(v)
            table.insert(POOL, v)
            ffi.gc(v, add_to_pool)
        end

        for _ = 1, POOL_SIZE do
            local v = CreateVector(0, 0, 0)
            add_to_pool(v)
        end
    end

    if timer.Simple then
        -- Lot's of addons cache Vectors when they load, and main reason for the pool is for temporary vectors, so we need to avoid them using the vectors inside the pool
        timer.Simple(0, initialize_vector_pool)
    else
        initialize_vector_pool()
    end
end

jit.markFFITypeAsGmodUserData(Vector(1, 1, 1))
