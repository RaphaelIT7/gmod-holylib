return {
    groupName = "HolyLib.InvalidateBoneCache",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.InvalidateBoneCache ).to.beA( "function" )
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
            name = "Errors instead of crashing when given a NULL entity",
            when = HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                expect( HolyLib.InvalidateBoneCache, NULL ).to.errWith( "bad argument #1 to '?' (Tried to use a NULL Entity!)" )
            end
        },
        {
            name = "Errors when given an Entity that isn't a CBaseAnimating",
            when = HolyLib_IsModuleEnabled("HolyLib") and IS_BASE_BRANCH,
            func = function()
                local ent = MakeTestEntity( "info_target" )

                expect( HolyLib.InvalidateBoneCache, ent ).to.errWith( "bad argument #1 to '?' (Tried use use an Entity that isn't a CBaseAnimating)" )

                SafeRemoveEntity( ent )
            end
        },
        {
            name = "Succeeds for a CBaseAnimating entity",
            when = HolyLib_IsModuleEnabled("HolyLib") and IS_BASE_BRANCH,
            func = function()
                local ent = MakeTestEntity( "prop_dynamic", nil, true )

                HolyLib.InvalidateBoneCache( ent )

                SafeRemoveEntity( ent )
            end
        },
    }
}
