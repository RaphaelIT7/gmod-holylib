local tab = stringtable.FindTable("client_lua_files")
if IsValid(tab) then
    local originalSize = 0
	local compressedSize = 0
	local uncompressedSize = 0
	local files = tab:GetAllStrings()
    local fileContents = {}
    table.remove(files, 1) -- first entry is "paths" - the string userdata store all lua search paths!
	for _, fileName in ipairs(files) do
        print(fileName)
        originalSize = originalSize + string.len(file.Read(fileName, "GAME")) -- Why does GMod store the full path even addons/xxx in the stringtable...
        local content = gmoddatapack.GetStoredCode(fileName)
        table.insert(fileContents, content)
		uncompressedSize = uncompressedSize + string.len(content)
		compressedSize = compressedSize + gmoddatapack.GetCompressedSize(fileName)
	end

    print("originalSize: " .. string.NiceSize(originalSize))
	print("uncompressedSize (stripped of comments & Server code): " .. string.NiceSize(uncompressedSize))
	print("compressedSize: " .. string.NiceSize(compressedSize))
	
    util.AsyncCompress(table.concat(fileContents, ""), 9, nil, function(res) -- HolyLib uses level 9 for compression to. GMod by default seems to use level 5
		print("compressedSize (all files in one compress): " .. string.NiceSize(string.len(res)))
    end)
end