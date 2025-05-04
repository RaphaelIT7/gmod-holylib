return {
    groupName = "VoiceData:__newindex",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").__newindex ).to.beA( "function" )
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
                expect( voiceData.test ).to.equal( "Hello World" )
                expect( voiceData:GetTable().test ).to.equal( "Hello World" )
            end
        },
    }
}
