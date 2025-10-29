return {
    groupName = "IGModAudioChannel:Play",
    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ).Play ).to.beA( "function" )
            end
        },
        {
            name = "Function is nil when module disabled",
            when = not HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ) ).to.beA( "nil" )
            end
        },
        -- Enums returned from GetState()
        -- https://wiki.facepunch.com/gmod/Enums/GMOD_CHANNEL
        {
            name = "Starts playback on valid channel",
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

                    channel:Play()
                    bass.Update()
                    expect( channel:GetState() ).to.equal( 1 )
                    
                    done()
                end )
            end
        },
        {
            name = "Resumes playback on a valid channel after pause",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "noplay"
        
                bass.PlayFile(filePath, flags, function(channel, errorCode, errorMsg)
                    expect( channel ).toNot.beNil()
                    expect( channel ).to.beValid()

                    channel:Play()
                    bass.Update()
                    expect( channel:GetState() ).to.equal( 1 )

                    channel:Pause()
                    bass.Update()
                    expect( channel:GetState() ).to.equal( 2 )

                    channel:Play()
                    bass.Update()
                    expect( channel:GetState() ).to.equal( 1 )
                    
                    done()
                end )
            end
        },
        {
            name = "Calling Play() twice is safe",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "noplay"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( channel ).to.beValid()

                    channel:Play()
                    channel:Play()
                    expect( channel:GetState() ).to.equal( 1 )
                    
                    done()
                end )
            end
        },
        {
            name = "Audio channel remains valid after calling Play()",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "noplay"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( channel ).to.beValid()

                    channel:Play()
                    expect( channel ).to.beValid()
                    
                    done()
                end )
            end
        },
    }
}