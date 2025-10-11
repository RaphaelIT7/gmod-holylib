return {
    groupName = "cvar.GetAll",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                expect( cvar.GetAll ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("cvars"),
            func = function()
                expect( cvar ).to.beA( "nil" )
            end
        },
        {
            name = "Properly returns a table with all convars",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                local convars = cvar.GetAll()

                expect( convars ).to.beA( "table" )
            end
        },
        {
            name = "Properly creates a buffer from a string",
            when = HolyLib_IsModuleEnabled("cvars"),
            func = function()
                HolyLib_RunPerformanceTest("cvar.GetAll", cvar.GetAll)
            end
        },
    }
}
