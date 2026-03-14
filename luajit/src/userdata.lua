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


local proxy = newproxy()
give_userdata_table(proxy)

proxy.test = print
proxy.helloWorld = "Test"

proxy:test()
print(proxy.helloWorld)

local test1_dat = {}
local test1 = newproxy()
debug.setmetatable(test1, {
	__newindex = function(self, a, b)
		rawset(test1_dat, a, b)
	end,
	lol = function()
		print("xD")
	end,
})

jit.flush()
local loopCount = 1000000
local a = os.clock()
for k=1, loopCount do
	test1.test = 123
end
print(os.clock() - a)

local test2 = newproxy()
give_userdata_table(test2)
jit.flush()
local a = os.clock()
for k=1, loopCount do
	test2.test = 123
end
print(test2.test)
print(os.clock() - a)