jit.off()
tab = {
	a = "lol"
}
print(tab.a)
table.setreadonly(tab, 1)
local status, err = pcall(function()
	tab.a = 123 // <--- should error
end)
print("error status:", status, err)
print(tab.a)