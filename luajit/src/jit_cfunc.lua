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

local loopCount = 10000000

jit.flush()
local nojit_func = test_cfunc()
local a = os.clock()
for k=1, loopCount do
	nojit_func(k)
end
print("Normal CFunc:", os.clock() - a)

jit.flush()
local jit_func = test_jitcfunc()
local a = os.clock()
for k=1, loopCount do
	jit_func(k)
end
print("JITd CFunc:", os.clock() - a)
print("Done")