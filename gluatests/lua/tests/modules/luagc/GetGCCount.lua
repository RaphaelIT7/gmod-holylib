return {
    groupName = "luagc.GetGCCount",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetGCCount ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc ).to.beA( "nil" )
            end
        },
        {
            name = "Returns a positive number when called without arguments",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetGCCount() ).to.beGreaterThan( 0 )
            end
        },
        {
            name = "Returns 0 when the target object is the current GC head",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                collectgarbage("stop")
                local head = luagc.GetCurrentGCHeadObject()
                local count = luagc.GetGCCount( head )
                collectgarbage("restart")

                expect( count ).to.equal( 0 )
            end
        },
        {
            name = "Counts up to but excludes the given target object",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                collectgarbage("stop")
                local head = luagc.GetCurrentGCHeadObject()
                local marker = {}
                local count = luagc.GetGCCount( head )
                collectgarbage("restart")

                expect( count ).to.equal( 1 )
            end
        },
        {
            name = "A non GC value (number) behaves like passing no target at all",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetGCCount( 5 ) ).to.equal( luagc.GetGCCount() )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                HolyLib_RunPerformanceTest("luagc.GetGCCount", function() luagc.GetGCCount() end)
            end
        },
    }
}
