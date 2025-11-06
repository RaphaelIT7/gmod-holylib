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
        -- https://www.un4seen.com/doc/#bass/BASS_ChannelIsActive.html
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

                    -- checking for BASS_ACTIVE_PAUSED (2) or BASS_ACTIVE_PAUSED_DEVICE (3)
                    -- Both can be a result of Pause()
                    expect( channel:GetState() ).to.beBetween( 2, 3 )
                    
                    done()
                end )
            end
        },
        {
            name = "calling Pause() twice is being handled and remains paused",
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
                    expect( channel:GetState() ).to.beBetween( 2, 3 ) -- checking for BASS_ACTIVE_PAUSED (2) or BASS_ACTIVE_PAUSED_DEVICE (3)

                    channel:Pause()
                    bass.Update()
                    expect( channel:GetState() ).to.beBetween( 2, 3 )
                    
                    done()
                end )
            end
        },
        {
            name = "Calling pause on an stopped channel remains stopped",
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