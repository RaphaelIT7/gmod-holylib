return {
    groupName = "VoiceData:CreateCopy",
    cases = {
        {
            name = "Function exists on meta table",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                expect( FindMetaTable("VoiceData").CreateCopy ).to.beA( "function" )
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
            name = "Properly creates a copy",
            when = HolyLib_IsModuleEnabled("voicechat"),
            func = function()
                local voiceData = voicechat.CreateVoiceData()

                voiceData:SetData( "Hello World" )
                voiceData:SetPlayerSlot( 12 )
                voiceData:SetProximity( false )

                local copy = voiceData:CreateCopy()

                expect( copy ).toNot.equal( voiceData )
                expect( copy:GetData() ).to.equal( voiceData:GetData() )
                expect( copy:GetLength() ).to.equal( voiceData:GetLength() )
                expect( copy:GetPlayerSlot() ).to.equal( voiceData:GetPlayerSlot() )
                expect( copy:GetProximity() ).to.equal( voiceData:GetProximity() )

                voiceData:SetPlayerSlot( 13 )
                expect( copy:GetPlayerSlot() ).toNot.equal( voiceData:GetPlayerSlot() )
                -- Ensure that modifying one of them won't change the other one (in case it bugs and they use the same pointer/VoiceData)
            end
        },
    }
}
