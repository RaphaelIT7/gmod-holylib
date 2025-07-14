return {
    groupName = "bass.PlayFile",
    cases = {
        {
            name = "Function exists globally",
            when = HolyLib_IsModuleEnabled("bass"),
            func = function()
            	expect( bass ).to.beA( "table" )
                expect( bass.PlayFile ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exists globally",
            when = not HolyLib_IsModuleEnabled("bass"),
            func = function()
                expect( bass ).to.beA( "nil" )
            end
        },
        {
            name = "Returns an Table object",
            when = HolyLib_IsModuleEnabled("entitylist"),
            async = true,
            timeout = 2,
            func = function()
                bass.PlayFile("sound/bass_testsound.wav", "", function(channel, errCode, errMsg)
                	print(channel, errCode, errMsg)
                	done()
                end)
            end
        },
    }
}
