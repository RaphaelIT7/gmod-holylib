return {
    groupName = "vprof.GetRoot",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("vprof"),
            func = function()
                expect( vprof.GetRoot ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("vprof"),
            func = function()
                expect( vprof ).to.beA( "nil" )
            end
        },
        {
            name = "Returns a VProfNode object",
            when = HolyLib_IsModuleEnabled("vprof"),
            func = function()
                local node = vprof.GetRoot()
                expect( node ).to.beA( "VProfNode" )
                expect( node:GetName() ).to.beA( "string" )
            end
        },
        {
            name = "Doesn't delete the live profiler node when garbage collected",
            when = HolyLib_IsModuleEnabled("vprof"),
            func = function()
                do
                    local node = vprof.GetRoot()
                    expect( node:GetName() ).to.beA( "string" )
                end

                collectgarbage() -- Would previously delete the engine's live root node causing hell

                local node = vprof.GetRoot()
                expect( node:GetName() ).to.beA( "string" )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("vprof"),
            func = function()
                HolyLib_RunPerformanceTest("vprof.GetRoot", function() vprof.GetRoot() end)
            end
        },
    }
}
