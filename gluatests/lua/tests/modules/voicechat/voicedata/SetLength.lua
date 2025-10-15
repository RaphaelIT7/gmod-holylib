return {
    groupName = "VoiceData:SetLength",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").SetLength ).to.beA( "function" )
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
            name = "Properly sets the length without removing data",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World" )
                voiceData:SetLength( 5 )

                expect( voiceData:GetLength() ).to.equal( 5 )
                expect( voiceData:GetData() ).to.equal( "Hello" )

                voiceData:SetLength( 11 ) -- We never removed the data, GetData just returns the specified length any anything after the length is ignored yet still exists

                expect( voiceData:GetLength() ).to.equal( 11 )
                expect( voiceData:GetData() ).to.equal( "Hello World" )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World" )

                HolyLib_RunPerformanceTest("VoiceData:SetLength", voiceData.SetLength, voiceData, 5)
            end
        },
    }
}
