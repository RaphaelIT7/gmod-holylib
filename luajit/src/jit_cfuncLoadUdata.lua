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

local loopCount = 100000000

jit.flush()
local val = 0
local jit_func = test_jitcfunc7_1()
local a = os.clock()
for k=1, loopCount do
	val = val + jit_func()
end
print("JITd CFunc:", os.clock() - a, val)

jit.flush()
local val = 0
local jit_func = test_jitcfunc7_2()
local a = os.clock()
for k=1, loopCount do
	val = val + jit_func()
end
print("JITd CFunc UData Load:", os.clock() - a, val)
print("Done")