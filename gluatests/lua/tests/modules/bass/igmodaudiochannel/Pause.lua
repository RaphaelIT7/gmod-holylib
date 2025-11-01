return {
    groupName = "IGModAudioChannel:Pause",
    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ).Pause ).to.beA( "function" )
            end
        },
        {
            name = "Function is nil when module disabled",
            when = not HolyLib_IsModuleEnabled("bass"),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ) ).to.beA( "nil" )
            end
        },
        -- Enums returned from GetState()
        -- https://wiki.facepunch.com/gmod/Enums/GMOD_CHANNEL
        {
            name = "Pauses an playback on a valid channel",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( channel ).to.beValid()

                    channel:Pause()
                    bass.Update()
                    expect( channel:GetState() ).to.equal( 2 )
                    
                    done()
                end )
            end
        },
        {
            name = "calling Pause() twice is safe and remains paused",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile(filePath, flags, function(channel, errorCode, errorMsg)
                    expect( channel ).toNot.beNil()
                    expect( channel ).to.beValid()

                    channel:Pause()
                    bass.Update()
                    expect( channel:GetState() ).to.equal( 2 )

                    channel:Pause()
                    bass.Update()
                    expect( channel:GetState() ).to.equal( 2 )
                    
                    done()
                end )
            end
        },
        {
            name = "Calling pause on a stopped and remains stopped",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "noplay"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( channel ).to.beValid()
                    expect( channel:GetState() ).to.equal( 0 )

                    channel:Pause()
                    expect( channel:GetState() ).to.equal( 0 )
                    
                    done()
                end )
            end
        },
    }
}