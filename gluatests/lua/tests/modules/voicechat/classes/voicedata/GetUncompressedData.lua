return {
    groupName = "VoiceData:GetUncompressedData",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").GetUncompressedData ).to.beA( "function" )
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
                --local voiceData = voicechat.CreateVoiceData()

                --expect( #voiceData:GetUncompressedData() ).to.equal( 0 )
            end
        },
    }
}
