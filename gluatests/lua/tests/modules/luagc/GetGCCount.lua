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
                jit.off()
                local head = luagc.GetCurrentGCHeadObject()
                local count = luagc.GetGCCount( head )
                jit.on()
                collectgarbage("restart")

                expect( count ).to.equal( 0 )
            end
        },
        {
            name = "Counts up to but excludes the given target object",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                collectgarbage("stop")
                jit.off()
                local head = luagc.GetCurrentGCHeadObject()
                local marker = {}
                local count = luagc.GetGCCount( head )
                jit.on()
                collectgarbage("restart")

                expect( count ).to.equal( 1 )
            end
        },
        {
            name = "A non GC value (number) behaves like passing no target at all",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
            	local firstCount = luagc.GetGCCount( 5 )
            	local secondCount = luagc.GetGCCount()
            	-- We don't do it inside the expect as the index methods may cause GC allocations!
                expect( firstCount ).to.equal( secondCount )
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
