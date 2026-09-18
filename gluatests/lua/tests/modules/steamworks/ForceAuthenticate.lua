return {
    groupName = "steamworks.ForceAuthenticate",
    cases = {
        {
            name = "Function exists on the built-in steamworks table",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.ForceAuthenticate ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exist",
            when = not HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.ForceAuthenticate ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when called without a userID",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.ForceAuthenticate ).to.errWith( "bad argument #1 to '?' (number expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non number userID",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.ForceAuthenticate, "string" ).to.errWith( "bad argument #1 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Returns false for a userID that belongs to no connected client",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                expect( steamworks.ForceAuthenticate( 999999 ) ).to.beFalse()
            end
        },
        {
            name = "Returns true and marks a real client as authenticated",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                local bot = MakeTestBot("Steamworks_ForceAuthenticate_Test")

                expect( steamworks.ForceAuthenticate( bot:UserID() ) ).to.beTrue()

                bot:Kick()
            end
        },
        {
            name = "Performance for a missing client",
            when = HolyLib_IsModuleEnabled("steamworks"),
            func = function()
                HolyLib_RunPerformanceTest("steamworks.ForceAuthenticate", function() steamworks.ForceAuthenticate( 999999 ) end)
            end
        },
    }
}
