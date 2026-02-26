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

jit.flush()
local jit_func = test_jitcfunc()
for k=1, 100 do
	jit_func(5, 10, 15)
end

jit.flush()
local jit_func2 = test_jitcfunc2()
for k=1, 100 do
	jit_func2("hello", "world")
end

jit.flush()
local proxy = newproxy()
local mt = {}
debug.setmetatable(proxy, mt)

local jit_func3 = test_jitcfunc3()
for k=1, 100 do
	local tab = jit_func3(proxy)
	print(mt, tab, mt == tab)
end

jit.flush()
local jit_func4 = test_jitcfunc4()
for k=1, 100 do
	jit_func4()
end