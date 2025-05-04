return {
    groupName = "VoiceData:GetLength",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").GetLength ).to.beA( "function" )
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

                expect( voiceData:GetLength() ).to.equal( 0 )
            end
        },
        {
            name = "Properly returns the length",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World" )
                expect( voiceData:GetLength() ).to.equal( 11 )
            end
        },
        {
            name = "Properly returns the set data",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World", 5 ) -- Length of 5 / everything after Hello is cut away
                expect( voiceData:GetLength() ).to.equal( 5 )
            end
        },
    }
}
