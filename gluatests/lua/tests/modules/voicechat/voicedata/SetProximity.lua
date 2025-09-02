return {
    groupName = "VoiceData:SetProximity",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").SetProximity ).to.beA( "function" )
            end
        },
        {
            name = "Metatable doesn't exist",
            when = not HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData") ).to.beA( "nil" )
            end
        },
        {
            name = "Properly sets the value",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetProximity( false )
                expect( voiceData:GetProximity() ).to.beFalse()
            end
        },
        {
            name = "Performance",
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                HolyLib_RunPerformanceTest("VoiceData:SetProximity", voiceData.SetProximity, voiceData, false)
            end
        },
    }
}
