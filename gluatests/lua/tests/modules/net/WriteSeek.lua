return {
    groupName = "net.WriteSeek",
    cases = {
        {
            name = "Function exists on the built-in net table",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.WriteSeek ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exist",
            when = not HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.WriteSeek ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when called without a position",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.WriteSeek ).to.errWith( "bad argument #1 to '?' (number expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non number position",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.WriteSeek, "string" ).to.errWith( "bad argument #1 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Errors when given a negative position",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.WriteSeek, -1 ).to.errWith( "bad argument #1 to '?' (Number is not allowed to be below 0!)" )
            end
        },
        {
            name = "Errors when there is no active net message being written",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.WriteSeek, 0 ).to.errWith( "Tried to use net.WriteSeek with no active net message!" )
            end
        },
        {
            name = "Doesn't error when seeking inside an active net message",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                util.AddNetworkString( "Net_WriteSeek_Test" )

                net.Start( "Net_WriteSeek_Test" )
                net.WriteLong( 1234 )
                net.WriteSeek( 8 )
                net.WriteLong( 5678 )
                net.Cancel()
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                util.AddNetworkString( "Net_WriteSeek_Test" )

                net.Start( "Net_WriteSeek_Test" )
                HolyLib_RunPerformanceTest("net.WriteSeek", function() net.WriteSeek(8) end)
                net.Cancel()
            end
        },
    }
}
