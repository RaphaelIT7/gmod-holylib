return {
    groupName = "VoiceData:SetUncompressedData",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").SetUncompressedData ).to.beA( "function" )
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
            name = "Does nothing if no data is given",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                --expect( voiceData:SetUncompressedData("") ).to.equal( 0 )
            end
        },
        {
            name = "Performance",
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                -- ToDo
                -- HolyLib_RunPerformanceTest("VoiceData:SetProximity", voiceData.SetProximity, voiceData, false)
            end
        },
    }
}
