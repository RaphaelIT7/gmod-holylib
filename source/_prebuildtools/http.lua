--[[
	tbl = {
		failed = function(reason) end,
		success = function(body) end,
		method = "GET POST HEAD PUT DELETE PATCH OPTIONS",
		url = "www.google.com",
		body = "",
		type = "text/plain; charset=utf-8",
		timeout = 60,
		headers = {
			["APIKey"] = "SaliBonani" 
		}
	}

	ToDo: Make this far better
]]

RemoveDir("http") -- Nuke old data!
CreateDir("http")

local last_added_request = os.time()
local requests = {}
function HTTP_WaitForAllInternal()
	local i = 0
	for key, tbl in pairs(requests) do
		i = i + 1

		local httpcontent = ReadFile(tbl.httpfile)
		if httpcontent and not (httpcontent == "") then
			local fileSize = string.len(httpcontent)
			local fileChanged = (tbl.fileSize or 0) ~= fileSize
			if fileChanged or (tbl.raceConditionTime and os.clock() < tbl.raceConditionTime) then
				tbl.fileSize = fileSize
				if fileChanged then
					tbl.raceConditionTime = os.clock() + 0.2 -- We add some delay since it can take a few miliseconds to finish outputting.
					print("Skipping result since it's sill being outputted (race condition)")
				end
				continue
			end

			table.remove(requests, key)
			local success = tbl.handle and tbl.handle:close()
			if success or not tbl.handle then
				if tbl.success then
					tbl.success(httpcontent)
				end
			else
				if tbl.failed then
					tbl.failed("Request failed: " .. success)
				end
			end
		end
	end

	if (os.time() - last_added_request) > 20 then
		print("HTTP Took way too long. Assuming something broke!")
		requests = {} -- Discard of this crap
		return false
	end

	return i > 0
end

function HTTP_WaitForAll()
	while HTTP_WaitForAllInternal() do end
end

local function CopyTable(input, references)
	local output = {}
	references = references or {} -- to prevent loops
	if references[input] then
		print("CopyTable was called with looping references!")
		return output
	end
	references[input] = true

	for key, value in pairs(input) do
		if type(value) == "table" then
			output[key] = CopyTable(value, references)
		else
			output[key] = value
		end
	end

	return output
end

local i = 0
function HTTP(inputTbl)
	i = i + 1

	local tbl = CopyTable(inputTbl) -- we don't want to modify the input table!
	local method = tbl.method or "GET"
	local url = tbl.url or ""
	local body = tbl.body or ""
	local contentType = tbl.type or ""
	local timeout = tbl.timeout or 15
	local headers = ""
	local params = ""
	if tbl.params then
		if type(tbl.params) == "string" then
			params = " --data-urlencode \"" .. tbl.params .. "\""
		else
			for _, param in ipairs(tbl.params) do
				params = params .. " --data-urlencode \"" .. param .. "\""
			end
		end
	end

	if tbl.headers then
		for key, value in pairs(tbl.headers) do
			headers = headers .. " -H \"" .. key .. ":" .. value .. "\""
		end
	end
	tbl.httpfile = "http/" .. i .. ".txt"
	--tbl.httpdonefile = "http/" .. i .. "_done.txt"

	local curlCommand = "curl -sb -X " .. method .. " " .. url .. params .. (not (contentType == "") and (" -H \"Content-Type:".. contentType .. "\"") or "") .. headers .. (body == "" and "" or (" --data-raw \"" .. body .. "\"")) .. " --max-time " .. timeout .. " > " .. tbl.httpfile --.. " && echo \"Done\" > " .. tbl.httpfile
	local handle = io.popen(curlCommand)
	tbl.handle = handle

	if not tbl.mode or tbl.mode == "async" then
		table.insert(requests, tbl)
	elseif tbl.mode == "sync" then
		handle:read('*all')
		handle:close()
		print("Result")
		local httpcontent = ReadFile(tbl.httpfile)
		if httpcontent and not (httpcontent == "") then
			local success = tbl.handle and tbl.handle:close()
			if success or not tbl.handle then
				if tbl.success then
					tbl.success(httpcontent)
				end
			else
				if tbl.failed then
					tbl.failed("Request failed: " .. success)
				end
			end
		end
	end

	last_added_request = os.time()
end

function HTTPDownload(tbl)
	i = i + 1

	local url = tbl.url or ""
	tbl.httpfile = tbl.file or "http/" .. i .. ".txt"
	local timeout = tbl.timeout or 15
	local headers = ""
	local params = ""
	if tbl.params then
		if type(tbl.params) == "string" then
			params = " --data-urlencode \"" .. tbl.params .. "\""
		else
			for _, param in ipairs(tbl.params) do
				params = params .. " --data-urlencode \"" .. param .. "\""
			end
		end
	end

	if tbl.headers then
		for key, value in pairs(tbl.headers) do
			headers = headers .. " -H \"" .. key .. ":" .. value .. "\""
		end
	end

	local curlCommand = "curl -L " .. url .. params .. headers .. " --max-time " .. timeout .. " -s -o \"" .. tbl.httpfile .. "\""
	local handle = io.popen(curlCommand)
	tbl.handle = handle

	table.insert(requests, tbl)
end

function JSONHTTP(tbl)
	local func = tbl.success
	tbl.success = function(body)
		local json = json.decode(body)
		if func then
			func(json)
		end
	end
	tbl.headers = tbl.headers or {}
	tbl.headers["Accept"] = "application/json"

	HTTP(tbl)
end