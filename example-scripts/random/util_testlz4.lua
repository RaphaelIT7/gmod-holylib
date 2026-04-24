local function perf(name, func, originalSize)
	local startTime = SysTime()

	local resultSize = func()

	local time = SysTime() - startTime
	print(name .. " - Took: " .. time .. "s")

	if originalSize then
		local ratio = originalSize / resultSize
		print(name .. " - Compression Ratio: " .. string.format("%.5f", ratio) .. ":1")
	end
end

local testFiles = {"10mb-examplefile-com.txt", "10mg-randomitzedexamplefile.txt"}
for _, fileName in ipairs(testFiles) do
    print("--------------------------")
    print("Testing File: " .. fileName)
    local data = file.Read(fileName, "BASE_PATH")
    print("Starting Data: " .. #data)

    perf("util.Compress", function()
        compressedData = util.Compress(data)
        print("GMOD - Compressed to: " .. #compressedData)
        return #compressedData
    end, #data)

    perf("util.Decompress", function()
        local decompressedData = util.Decompress(compressedData)
        print("GMOD - Decompress to: " .. #decompressedData)
        return #decompressedData
    end, #data)

    perf("util.CompressLZ4", function()
        compressedDataLZ4 = util.CompressLZ4(data)
        print("LZ4 - Compressed to: " .. #compressedDataLZ4)
        return #compressedDataLZ4
    end, #data)

    perf("util.DecompressLZ4", function()
        local decompressedData = util.DecompressLZ4(compressedDataLZ4)
        print("LZ4 - Decompress to: " .. #decompressedData)
        return #decompressedData
    end, #data)
    print("--------------------------")
end