return {
    groupName = "VoiceData:GetData",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").GetData ).to.beA( "function" )
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
            name = "Does nothing if no data is set",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                expect( #voiceData:GetData() ).to.equal( 0 )
            end
        },
        {
            name = "Properly returns the set Data",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World" )
                expect( voiceData:GetData() ).to.equal( "Hello World" )
            end
        },
        {
            name = "Properly returns the set data",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World", 5 ) -- Length of 5 / everything after Hello is cut away
                expect( voiceData:GetData() ).to.equal( "Hello" )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World", 5 ) -- Length of 5 / everything after Hello is cut away

                HolyLib_RunPerformanceTest("VoiceData:GetData", voiceData.GetData, voiceData)
            end
        },
    }
}
