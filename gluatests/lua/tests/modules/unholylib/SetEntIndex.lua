return {
    groupName = "unholylib.SetEntIndex",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                expect( unholylib.SetEntIndex ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                expect( unholylib ).to.beA( "nil" )
            end
        },
        {
            name = "Errors instead of crashing when given an invalid Entity",
            when = HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                expect( unholylib.SetEntIndex, NULL, 0 ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Errors when the given index is too negative",
            when = HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                local ent = MakeTestEntity()

                expect( unholylib.SetEntIndex, ent, -2 ).to.errWith( "bad argument #2 to '?' (Index is too large!)" )

                SafeRemoveEntity( ent )
            end
        },
        {
            name = "Errors when the given index is too large",
            when = HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                local ent = MakeTestEntity()

                expect( unholylib.SetEntIndex, ent, 999999 ).to.errWith( "bad argument #2 to '?' (Index is too large!)" )

                SafeRemoveEntity( ent )
            end
        },
        {
            name = "Turns the entity into a server-only entity when given -1",
            when = HolyLib_IsModuleEnabled("unholylib"),
            func = function()
                local ent = MakeTestEntity()
                local oldIndex = ent:EntIndex()
                expect( oldIndex ).to.beGreaterThan( 0 )

                unholylib.SetEntIndex( ent, -1 )

                -- GMod does not show -1 but rather 0 (There is no good way to tell if something is server only / has no edict)
                expect( ent:EntIndex() ).to.equal( 0 )
                expect( IsValid( ent ) ).to.beTrue()

                SafeRemoveEntity( ent )
            end
        },
    }
}
