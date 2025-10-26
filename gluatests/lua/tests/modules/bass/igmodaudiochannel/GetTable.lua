return {
    groupName = "IGModAudioChannel:GetTable",
    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ).GetTable ).to.beA( "function" )
            end
        },
        {
            name = "Function is nil when module disabled",
            when = not HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ) ).to.beA( "nil" )
            end
        },
        {
            name = "Sets the right value",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile(filePath, flags, function(channel, errorCode, errorMsg)

                    channel.test = "Hello World"
                    expect( channel:GetTable().test ).to.equal( "Hello World" )
                    
                    channel:GetTable().test = "Hello World 2"
                    expect( channel.test ).to.equal( "Hello World 2" )
                    
                    done()
                end)
            end
        },
    }
}
