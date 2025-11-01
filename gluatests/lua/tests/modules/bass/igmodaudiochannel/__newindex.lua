return {
    groupName = "IGModAudioChannel:__newindex",
    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ).__newindex ).to.beA( "function" )
            end
        },
        {
            name = "Function is nil when module disabled",
            when = not HolyLib_IsModuleEnabled("bass"),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ) ).to.beA( "nil" )
            end
        },
        {
            name = "Sets the new index value on channel",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    channel.test = "Hello World"
                    expect( channel.test ).to.equal( "Hello World" )
                    expect( channel:GetTable().test ).to.equal( "Hello World" )  
                    
                    done()
                end )
            end
        },
    }
}