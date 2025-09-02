return {
    groupName = "VoiceData:SetPlayerSlot",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").SetPlayerSlot ).to.beA( "function" )
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
            name = "Properly sets the slot",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetPlayerSlot(12)
                expect( voiceData:GetPlayerSlot() ).to.equal( 12 )
            end
        },
        {
            name = "Performance",
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                HolyLib_RunPerformanceTest("VoiceData:SetPlayerSlot", voiceData.SetPlayerSlot, voiceData, 12)
            end
        },
    }
}
