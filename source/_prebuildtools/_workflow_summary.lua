--[[
	This file is called by our workflow to create a summary after all builds & tests finished.
]]

dofile("utils.lua")

json = require("json")
require("http")

-- Let's fetch the current run data.
function FetchLokiResults(github_repository, runNumber, callback, host, apikey)
	JSONHTTP({
		blocking = true,
		failed = function(reason)
			print("Failed to get performance results from Loki for " .. runNumber .. "!", reason)
		end,
		success = function(json)
			print("Successfully got performance results from Loki for " .. runNumber .. " :3")
			callback(json)
		end,
		method = "GET",
		url = host .. "/loki/api/v1/query_range",
		params = {
			"query={run_number=\\\"" .. runNumber .. "\\\",repository=\\\"" .. github_repository .. "\\\"}",
			"since=30d",
			"limit=1000",
		},
		headers = {
			["X-Api-Key"] = apikey
		},
	})
end

function FetchFromLoki(github_repository, runNumber, host, apikey)
	local currentLokiResults = {}
	FetchLokiResults(github_repository, runNumber, function(jsonTable)
		currentLokiResults = jsonTable
	end, host, apikey)

	local lastLokiResults = {} -- Results of the last run.
	local lastLokiRun = -1
	nextLokiSearchID = runNumber - 1
	while (lastLokiRun == -1) and ((runNumber - nextLokiSearchID) < 100 and nextLokiSearchID > 0) do -- NUKE IT >:3
		local lokiID = nextLokiSearchID
		FetchLokiResults(github_repository, lokiID, function(jsonTable)
			if not jsonTable or not jsonTable.data or not jsonTable.data.result or #jsonTable.data.result == 0 then -- Useless!
				print("Skipping results for " .. lokiID .. " since the data is useless")
				return
			end

			if nextLokiSearchID < lastLokiRun then -- We got newer results already!
				print("Skipping results for " .. lokiID .. " since we got newer data")
				return
			end

			-- We got newer shit :D
			-- NOTE: This code previously was intented to be nuked in a for k=1, 100 loop though Loki's CPU usage spiked to 900% for 15 seconds :sob:
			lastLokiRun = lokiID
			lastLokiResults = jsonTable
			print("Found the last performance results " .. lokiID)
		end, host, apikey)

		HTTP_WaitForAll() -- This will also wait in the first iteration for our currentLokiResults
		nextLokiSearchID = nextLokiSearchID - 1
	end

	if lastLokiRun == -1 or not lastLokiResults or not lastLokiResults.data or not lastLokiResults.data.result or #lastLokiResults.data.result == 0 then
		WriteFile("generated_summary.md", "Failed to fetch previous results!")
		error("Failed to fetch previous results!")
		return
	end

	if not currentLokiResults or not currentLokiResults.data or not currentLokiResults.data.result or #currentLokiResults.data.result == 0 then
		WriteFile("generated_summary.md", "Failed to fetch results!")
		error("Failed to fetch results!")
		return
	end

	return currentLokiResults, lastLokiResults, lastLokiRun
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
	local lokiData = lokiResults.data.result[1]
	for _, lokiEntry in ipairs(lokiData.values) do
		local logEntry = json.decode(lokiEntry[2])
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
local loki_public_host = settings[2]
local loki_host = settings[3]
local loki_api = settings[4]
local run_number = tonumber(settings[5])

local usingPublic = (not loki_host or loki_host == "") and (not loki_api or loki_api == "")
if not run_number then
	WriteFile("generated_summary.md", "Got no results")
	error("Missing input data!")
	return
end

local currentLokiResults, lastLokiResults, lastLokiRun = FetchFromLoki(github_repo, run_number, usingPublic and loki_public_host or loki_host, loki_api)
local currentResults = CalculateMergedResults(currentLokiResults)
local previousResults = CalculateMergedResults(lastLokiResults)

PrintTable(currentResults)
PrintTable(previousResults)

local differences = CalculateDifferences(currentResults, previousResults)
local markdown = GenerateMarkdown(currentResults, run_number, lastLokiRun)
WriteFile("generated_summary.md", markdown)

PrintTable(currentResults)