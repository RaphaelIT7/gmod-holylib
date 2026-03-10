jit.off()

print(debug.gethook())

local testFunc = function() end
debug.sethook(testFunc, "l")
print(debug.gethook() == debug.getregistry()[0]) -- Shouldn't happen! registry index 0 is used by luaL_unref and will override!
for k, v in pairs(debug.getregistry()) do print(k, v) end

-- This issue doesn't happen when testing with the luajit.exe but when we use holylib it dies???