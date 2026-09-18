return {
    groupName = "luagc.GetAllGCObjects",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetAllGCObjects ).to.beA( "function" )
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
            name = "Returns a non-empty table when called without arguments",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local list = luagc.GetAllGCObjects()

                expect( list ).to.beA( "table" )
                expect( #list ).to.beGreaterThan( 0 )
            end
        },
        {
            name = "The returned table always contains itself as the first entry (documented quirk)",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                collectgarbage("stop")
                local head = luagc.GetCurrentGCHeadObject()
                local list = luagc.GetAllGCObjects( head )
                collectgarbage("restart")

                expect( list[1] ).to.equal( list )
            end
        },
        {
            name = "Lists exactly the objects created after the given target, newest first",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                collectgarbage("stop")
                local head = luagc.GetCurrentGCHeadObject()
                local marker = {}
                local list = luagc.GetAllGCObjects( head )
                collectgarbage("restart")

                table.remove( list, 1 )

                expect( list[1] ).to.equal( marker )
            end
        },
        {
            name = "A non GC value (number) behaves like passing no target at all",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local list = luagc.GetAllGCObjects( 5 )

                expect( list ).to.beA( "table" )
                expect( #list ).to.beGreaterThan( 0 )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                HolyLib_RunPerformanceTest("luagc.GetAllGCObjects", function() luagc.GetAllGCObjects() end)
            end
        },
    }
}
