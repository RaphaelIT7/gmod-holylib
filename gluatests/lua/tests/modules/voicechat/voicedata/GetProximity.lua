return {
    groupName = "VoiceData:GetProximity",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").GetProximity ).to.beA( "function" )
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
            name = "Defaults to true",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                expect( voiceData:GetProximity() ).to.beTrue()
            end
        },
        {
            name = "Properly returns the set value",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetProximity( false )
                expect( voiceData:GetProximity() ).to.beFalse()
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World", 5 ) -- Length of 5 / everything after Hello is cut away

                HolyLib_RunPerformanceTest("VoiceData:GetProximity", voiceData.GetProximity, voiceData)
            end
        },
    }
}
