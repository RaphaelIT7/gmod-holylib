return {
    groupName = "CHLTVClient:__tostring",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( FindMetaTable("CHLTVClient").__tostring ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                expect( FindMetaTable("CHLTVClient") ).to.beA( "nil" )
            end
        },
        {
            name = "Returns the NULL string when self is not a valid CHLTVClient",
            when = HolyLib_IsModuleEnabled("sourcetv"),
            func = function()
                local tostring = FindMetaTable("CHLTVClient").__tostring

                expect( tostring() ).to.equal( "CHLTVClient [NULL]" )
                expect( tostring( "not a client" ) ).to.equal( "CHLTVClient [NULL]" )
                expect( tostring( NULL ) ).to.equal( "CHLTVClient [NULL]" )
            end
        },
    }
}
