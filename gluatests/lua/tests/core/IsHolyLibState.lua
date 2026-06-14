local function IsHolyLib()
	return string.EndsWith(jit.version, "(HolyLib's Custom Build)")
end

return {
    groupName = "HolyLib manages to properly Push HolyLib UserData to Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.IsHolyLibState function existent",
            func = function()
                expect( _HOLYLIB_CORE.IsHolyLibState ).to.beA( "function" )
            end
        },
        {
            name = "Correctly recognizes the state",
            func = function()
                expect( _HOLYLIB_CORE.IsHolyLibState() ).to.equal( IsHolyLib() )

                local suc, ret = coroutine.resume(coroutine.create(function() return _HOLYLIB_CORE.IsHolyLibState() end))
                expect( suc ).to.beTrue()
                expect( ret ).to.equal( IsHolyLib() )
            end
        },
    }
}
