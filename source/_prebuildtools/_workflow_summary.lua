--[[
	This file is called by our workflow to create a summary after all builds & tests finished.
]]

dofile("utils.lua")

json = require("json")
require("http")

-- Let's fetch the current run data.
function FetchHolyLogsResults(github_repository, runNumber, callback, host, apikey)
	HTTP({
		failed = function(reason)
			print("Failed to get performance results from HolyLogs for " .. runNumber .. "!", reason)
		end,
		success = function(responseBody)
			print("Successfully got performance results from HolyLogs for " .. runNumber .. " :3")
			if string.len(responseBody) < 3 then
				callback(nil)
				return
			end

			local function SubUntilNull(str, startPos)
				for k=startPos, string.len(str) do
					if string.byte(responseBody, k, k) == 0 then
						return string.sub(str, startPos, k-1), k
					end
				end

				return string.sub(str, startPos), string.len(str)
			end

			local entries = {}
			local pos = 0
			while pos < string.len(responseBody) do
				local lengthStr, newPos = SubUntilNull(responseBody, pos)
				pos = newPos + 1

				local length = tonumber(lengthStr)
				if not length then
					print("Failed to parse number (" .. lengthStr .. ")")
					return
				end

				local data = string.sub(responseBody, pos, pos + length - 1)
				pos = pos + length + 1 -- Every entry too is ended by a null byte that we can skip

				-- if string.len(data) != length then print("Length for entry doesn't match!") end

				table.insert(entries, data)
			end

			callback(entries)
		end,
		method = "GET",
		url = host .. "/GetEntries",
		headers = {
			["entryIndex"] = github_repository .. "___" .. runNumber, -- Unique key for this run.
			["X-Api-Key"] = apikey
		},
	})
end

function FetchFromHolyLogs(github_repository, runNumber, host, apikey)
	local currentHolyLogsResults = {}
	FetchHolyLogsResults(github_repository, runNumber, function(jsonTable)
		currentHolyLogsResults = jsonTable
	end, host, apikey)

	local lastHolyLogsResults = {} -- Results of the last run.
	local lastRun = -1
	nextSearchID = runNumber - 1
	while (lastRun == -1) and ((runNumber - nextSearchID) < 100 and nextSearchID > 0) do -- NUKE IT >:3
		local searchID = nextSearchID
		FetchHolyLogsResults(github_repository, searchID, function(jsonTable)
			if not jsonTable then
				print("Skipping " .. searchID .. " since it has no data")
				return
			end

			if nextSearchID < lastRun then -- We got newer results already!
				print("Skipping results for " .. searchID .. " since we got newer data")
				return
			end

			-- We got newer shit :D
			-- NOTE: This code previously was intented to be nuked in a for k=1, 100 loop though Loki's CPU usage spiked to 900% for 15 seconds :sob:
			-- We could go back now since we no longer use Loki
			lastRun = searchID
			lastHolyLogsResults = jsonTable
			print("Found the last performance results " .. searchID)
		end, host, apikey)

		HTTP_WaitForAll() -- This will also wait in the first iteration for our currentHolyLogsResults
		nextSearchID = nextSearchID - 1
	end

	if lastRun == -1 or not lastHolyLogsResults then
		WriteFile("generated_summary.md", "Failed to fetch previous results!")
		error("Failed to fetch previous results!")
		return
	end

	if not currentHolyLogsResults then
		WriteFile("generated_summary.md", "Failed to fetch results!")
		error("Failed to fetch results!")
		return
	end

	return currentHolyLogsResults, lastHolyLogsResults, lastRun
end

--[[
	The loki entry will be the following structure:

	[1] = (string) nano second time of the log entry
	[2] = (string) json string containing our data

	resultTable structure:
		[branch name] = (table) {
			[function name] = {
				totalCalls = total calls,
				timePerCall = average time per calls,
				runCount = how many times performance results got collected/now many we merged together,
			}
		}

	logEntry structure:
		totalCalls = number
		totalTime = number
		timePerCall = number (deprecated & removed. use totalTime / totalCalls)
		gmodBranch = string
		name = string
]]

function CalculateMergedResults(lokiResults)
	local resultTable = {}
	for _, jsonEntry in ipairs(lokiResults) do
		local logEntry = json.decode(jsonEntry)
		if not logEntry then continue end

		local branchResults = resultTable[logEntry.gmodBranch]
		if not branchResults then
			branchResults = {}
			resultTable[logEntry.gmodBranch] = branchResults
		end

		local funcResults = branchResults[logEntry.name]
		if not funcResults then
			funcResults = {}
			branchResults[logEntry.name] = funcResults

			funcResults.totalCalls = tonumber(logEntry.totalCalls) or 0
			funcResults.totalTime = tonumber(logEntry.totalTime) or 0
			funcResults.runCount = 1
			continue
		end

		funcResults.totalCalls = funcResults.totalCalls + (tonumber(logEntry.totalCalls) or 0)
		funcResults.totalTime = funcResults.totalTime + (tonumber(logEntry.totalTime) or 0)
		funcResults.runCount = funcResults.runCount + 1
	end

	return resultTable
end

--[[
	Now we fill the currentResults table with more data like speed difference between it's runs and the previous ones.

	Fields that are added to currentResults:
		diffTimePerCall - number
		diffTotalCalls - number

	We also fill in these fields:
		timePerCall - number
]]
function CalculateDifferences(currentResults, previousResults)
	for branch, funcs in pairs(currentResults) do
		local prevFuncs = previousResults[branch] or {}

		for functionName, funcResults in pairs(funcs) do
			local prevFuncResults = prevFuncs[functionName]
			if prevFuncResults then
				prevFuncResults.timePerCall = prevFuncResults.totalTime / prevFuncResults.totalCalls
			end

			funcResults.timePerCall = funcResults.totalTime / funcResults.totalCalls
			funcResults.diffTimePerCall = prevFuncResults and (funcResults.timePerCall / prevFuncResults.timePerCall) or 1
		end
	end
end

-- Markdown time :D
function GenerateMarkdown(results, currentRun, previousRun)
	local markdown = {}
	table.insert(markdown, "# Results")
	table.insert(markdown, "Current run: " .. currentRun .. [[<br>
Previous run: ]] .. previousRun .. "<br>")

	for branch, funcs in pairs(results) do
		table.insert(markdown, "")
		table.insert(markdown, "# Branch: " .. branch)
		table.insert(markdown, "| Function Name | Total Calls | Time Per Call | Difference to Previous |")
		table.insert(markdown, "| ------------- | ----------- | ------------- | ---------------------- |")

		for funcName, funcResults in pairs(funcs) do
			if math.abs(1 - funcResults.diffTimePerCall) > 0.20 then -- It may fluxuate between builds because of different runners/hardware but it shouldn't worsen by more than 20%
				print("::warning title=" .. funcName .. "::Performance is worse beyond expectation")
			end

			table.insert(markdown, "| " .. funcName .. " | " .. funcResults.totalCalls .. " | " .. funcResults.timePerCall .. " | " .. funcResults.diffTimePerCall .. "x |")
		end
	end

	return table.concat(markdown, [[

]])
end

local settings = {...}
local github_repo = settings[1]
local holylogs_public_host = settings[2]
local holylogs_host = settings[3]
local holylogs_api = settings[4]
local run_number = tonumber(settings[5])

local usingPublic = (not holylogs_host or holylogs_host == "") and (not holylogs_api or holylogs_api == "")
if not run_number then
	WriteFile("generated_summary.md", "Got no results")
	error("Missing input data!")
	return
end

local currentHolyLogsResults, lastHolyLogsResults, lastRun = FetchFromHolyLogs(github_repo, run_number, usingPublic and holylogs_public_host or holylogs_host, holylogs_api)
local currentResults = CalculateMergedResults(currentHolyLogsResults)
local previousResults = CalculateMergedResults(lastHolyLogsResults)

PrintTable(currentResults)
PrintTable(previousResults)

local differences = CalculateDifferences(currentResults, previousResults)
local markdown = GenerateMarkdown(currentResults, run_number, lastRun)
WriteFile("generated_summary.md", markdown)

PrintTable(currentResults)