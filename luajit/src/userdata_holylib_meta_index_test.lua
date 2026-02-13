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

local noJIT = false

local proxy = newproxy()
debug.userdata_setusertable(proxy, true)

if noJIT then
	jit.off()
end

proxy.test = print
proxy.helloWorld = "Test"

proxy:test()
print(proxy.helloWorld)

jit.flush()
local loopCount = 10000000000 / (noJIT and 100 or 1)

local test3 = newproxy()
debug.userdata_setusertable(test3, true)
debug.userdata_setmetaaccess(test3, true)
debug.setmetatable(test3, {
	testfunc = function() end,
})

jit.flush()
local a = os.clock()
for k=1, loopCount do
	local val = test3.testfunc
end
print("UserData meta __index:", os.clock() - a)