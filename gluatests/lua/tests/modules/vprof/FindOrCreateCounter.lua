return {
    groupName = "vprof.FindOrCreateCounter",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("vprof"),
            func = function()
                expect( vprof.FindOrCreateCounter ).to.beA( "function" )
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
            name = "Returns a working VProfCounter object",
            when = HolyLib_IsModuleEnabled("vprof"),
            func = function()
                local counter = vprof.FindOrCreateCounter("HolyLib_UnitTest_Counter")
                expect( counter ).to.beA( "VProfCounter" )
                expect( counter:GetName() ).to.equal( "HolyLib_UnitTest_Counter" )
            end
        },
        {
            name = "Doesn't corrupt memory when garbage collected",
            when = HolyLib_IsModuleEnabled("vprof"),
            func = function()
                do
                    local counter = vprof.FindOrCreateCounter("HolyLib_UnitTest_Counter2")
                    expect( counter:GetName() ).to.equal( "HolyLib_UnitTest_Counter2" )
                end

                collectgarbage() -- Would previously delete the inline userdata which would corrupt the heap

                local counter = vprof.FindOrCreateCounter("HolyLib_UnitTest_Counter2")
                expect( counter:GetName() ).to.equal( "HolyLib_UnitTest_Counter2" )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("vprof"),
            func = function()
                HolyLib_RunPerformanceTest("vprof.FindOrCreateCounter", function() vprof.FindOrCreateCounter("HolyLib_UnitTest_Counter") end)
            end
        },
    }
}
