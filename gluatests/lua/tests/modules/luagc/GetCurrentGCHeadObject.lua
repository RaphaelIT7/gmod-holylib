return {
    groupName = "luagc.GetCurrentGCHeadObject",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetCurrentGCHeadObject ).to.beA( "function" )
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
            name = "Returns a non-nil value",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetCurrentGCHeadObject() ).toNot.beNil()
            end
        },
        {
            name = "Reflects the most recently created GC object",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                collectgarbage("stop")
                local marker = {}
                local head = luagc.GetCurrentGCHeadObject()
                collectgarbage("restart")

                expect( head ).to.equal( marker )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                HolyLib_RunPerformanceTest("luagc.GetCurrentGCHeadObject", function() luagc.GetCurrentGCHeadObject() end)
            end
        },
    }
}
