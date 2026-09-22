return {
    groupName = "HolyLib.UserMessageBegin",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.UserMessageBegin ).to.beA( "function" )
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
            name = "Errors when called without a message name",
            when = HolyLib_IsModuleEnabled("HolyLib") and IS_BASE_BRANCH,
            func = function()
                local filter = RecipientFilter()

                expect( HolyLib.UserMessageBegin, filter ).to.errWith( "bad argument #2 to '?' (string expected, got no value)" )
            end
        },
        {
            name = "Disabled on x86-64",
            when = HolyLib_IsModuleEnabled("HolyLib") and IS_64BIT_BRANCH,
            func = function()
                local filter = RecipientFilter()

                expect( HolyLib.UserMessageBegin, filter, "GameTitle" ).to.errWith( "This GMod branch is not supported currently" )
            end
        },
        {
            name = "Returns a bf_write buffer that can be written to and finished",
            when = HolyLib_IsModuleEnabled("HolyLib") and HolyLib_IsModuleEnabled("bitbuf") and IS_BASE_BRANCH,
            func = function()
                local filter = RecipientFilter()

                local bf = HolyLib.UserMessageBegin( filter, "GameTitle" )

                local ok, err = pcall( function()
                    expect( bf ).to.beA( "bf_write" )
                    expect( bf:IsValid() ).to.beTrue()
                end )

                HolyLib.MessageEnd()

                if not ok then
                    error( err, 0 )
                end
            end
        },
    }
}
