return {
    groupName = "VoiceData:GetPlayerSlot",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").GetPlayerSlot ).to.beA( "function" )
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
            name = "Defaults to 0",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                expect( voiceData:GetPlayerSlot() ).to.equal( 0 )
            end
        },
        {
            name = "Properly returns the slot",
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

                voiceData:SetData( "Hello World", 5 ) -- Length of 5 / everything after Hello is cut away

                HolyLib_RunPerformanceTest("VoiceData:GetPlayerSlot", voiceData.GetPlayerSlot, voiceData)
            end
        },
    }
}
