return {
    groupName = "voicechat.CreateVoiceData",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( voicechat.CreateVoiceData ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( voicechat ).to.beA( "nil" )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                HolyLib_RunPerformanceTest("voicechat.CreateVoiceData", voicechat.CreateVoiceData)
            end
        },
    }
}
