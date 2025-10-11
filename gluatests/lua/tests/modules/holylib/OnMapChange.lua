return {
    groupName = "HolyLib:OnMapChange",
    cases = {
        {
            name = "Hook is called with correct map name",
            when = file.Exists("HookOnMapChangeData.txt", "DATA"),
            func = function(state)
                local data = file.Read("HookOnMapChangeData.txt", "DATA")
                expect(data).to.exist()

                local lines = string.Explode("\n", data)
                expect(#lines).to.beGreaterThan(1)

                local oldLevel = lines[1]
                local levelName = lines[2]

                expect(levelName).to.beA("string")
                expect(levelName).to.equal(oldLevel)
                expect(levelName).to.equal(game.GetMap())
            end,
        },
    }
}
