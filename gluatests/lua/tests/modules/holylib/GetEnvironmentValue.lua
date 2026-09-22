return {
    groupName = "HolyLib.GetEnvironmentValue",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.GetEnvironmentValue ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when called without a string argument",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local ok, err = pcall( HolyLib.GetEnvironmentValue )
                expect( ok ).to.beFalse()

                local unsafeMsg = "Tried to use a unsafe code function while -holylib_allowunsafe is not active!"
                local argMsg = "bad argument #1 to '?' (string expected, got no value)"
                expect( err == unsafeMsg or err == argMsg ).to.beTrue()
            end
        },
        {
            name = "Returns nil for an unset environment variable, or throws the unsafe-code error",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local ok, result = pcall( HolyLib.GetEnvironmentValue, "HolyLib_GetEnvironmentValue_Test" )

                if ok then
                    expect( result ).to.equal( "" )
                else
                    expect( result ).to.equal( "Tried to use a unsafe code function while -holylib_allowunsafe is not active!" )
                end
            end
        },
        {
            name = "Returns a string for a known environment variable, or throws the unsafe-code error",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local ok, result = pcall( HolyLib.GetEnvironmentValue, "PATH" )

                if ok then
                    expect( result ).to.beA( "string" )
                else
                    expect( result ).to.equal( "Tried to use a unsafe code function while -holylib_allowunsafe is not active!" )
                end
            end
        },
    }
}
