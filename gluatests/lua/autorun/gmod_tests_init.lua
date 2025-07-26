include( "gmod_tests/sh_init.lua" )

hook.Add("HolyLib:OnMapChange", "HookOnMapChangeTest", function(levelName, levelLandmark)
	local currentMap = game.GetMap()

	if not file.Exists("HookOnMapChangeData.txt", "DATA") then
		file.Write("HookOnMapChangeData.txt", currentMap .. "\n" .. levelName .. "\n" .. levelLandmark)
	end
end)

-- We change the level once and run everything again as in rare cases a crash might only ocurr after a map change.
hook.Add("GLuaTest_Finished", "ChangeLevel", function()
	if not file.Exists("waschanged.txt", "DATA") then
		file.Write("waschanged.txt", "yes")

		RunConsoleCommand("changelevel", game.GetMap())
	end
end)
