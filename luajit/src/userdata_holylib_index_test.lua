jit.util = require("jit.util")
function generate_trace()
	jit.opt.start("hotloop=1", "hotexit=1")
	jit.flush()

	jit.attach(function(what, traceno, func, pc, exitno)
		if exitno ~= nil then what = "abort" end

		local src = "<unknown>"
		if type(func) == "function" or type(func) == "proto" then
			local info = jit.util.funcinfo(func)
			if info then
				src = string.format("%s:%d", info.source or "C", info.linedefined or 0)
			end
		elseif func ~= nil then
			src = tostring(func)
		end

		print(string.format("[TRACE] %-6s %s %-35s pc=%s exit=%s",
			tostring(what),
			tostring(traceno or "?"),
			tostring(src),
			tostring(pc or "-"),
			tostring(exitno or "-")
		))
	end, "trace")
end

generate_trace()

local noJIT = true

local proxy = newproxy()
debug.userdata_setusertable(proxy, true)

if noJIT then
	jit.off()
end

proxy.test = print
proxy.helloWorld = "Test"

proxy:test()
print(proxy.helloWorld)

local test1_dat = {}
local test1 = newproxy()
debug.setmetatable(test1, {
	__index = function(self, key)
		return rawget(debug.getfenv(self), key)
	end,
	__newindex = function(self, key, val)
		rawset(debug.getfenv(self), key, val)
	end,
})

jit.flush()
local loopCount = 1000000000 / (noJIT and 100 or 1)
local a = os.clock()
for k=1, loopCount/50 do // Less since I just want my test to work! I remove this if I need real comparison
	test1.test = 123
end
print("Normal Meta __newindex:", os.clock() - a)

jit.flush()
local a = os.clock()
for k=1, loopCount/50 do
	local val = test1.test
end
print("Normal Meta __index:", os.clock() - a)

jit.flush()
local test2 = {}
local a = os.clock()
for k=1, loopCount do
	test2.test = 123
end
print("Table __newindex:", os.clock() - a)

jit.flush()
local a = os.clock()
for k=1, loopCount do
	local val = test2.test
end
print("Table __index:", os.clock() - a)

local test3 = newproxy()
debug.userdata_setusertable(test3, true)
jit.flush()
local a = os.clock()
for k=1, loopCount do
	test3.test = 123
end
print("UserData __newindex:", os.clock() - a)

jit.flush()
local a = os.clock()
for k=1, loopCount do
	local val = test3.test
end
print("UserData __index:", os.clock() - a)

// Got asked to test this to compare - https://github.com/Srlion/gmodx-rs/blob/master/gmodx%2Fsrc%2Flua%2Fuserdata%2Fmod.rs#L40

local getmetatable = getmetatable
local STORE = setmetatable({}, { __mode = 'k' })
local function __index(self, k)
    local store = STORE[self]
    if store then
        local v = store[k]
        if v ~= nil then
            return v
        end
    end
    return getmetatable(self)[k]
end
local function __newindex(self, k, v)
    local store = STORE[self]
    if not store then
        STORE[self] = {
            [k] = v
        }
        return
    end
    store[k] = v
end

local test4 = newproxy()
debug.setmetatable(test4, {
	__index = __index,
	__newindex = __newindex,
})

jit.flush()
local a = os.clock()
for k=1, loopCount do
	test4.test = 123
end
print("Srlion UserData __newindex:", os.clock() - a)

jit.flush()
local a = os.clock()
for k=1, loopCount do
	local val = test4.test
end
print("Srlion UserData __index:", os.clock() - a)