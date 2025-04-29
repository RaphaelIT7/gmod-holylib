return {
    groupName = "VoiceData:GetTable",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").GetTable ).to.beA( "function" )
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
            name = "Sets the right value",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData.test = "Hello World"
                expect( voiceData:GetTable().test ).to.equal( "Hello World" )

                voiceData:GetTable().test = "Hello World 2"
                expect( voiceData.test ).to.equal( "Hello World 2" )
            end
        },
    }
}
