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
                    -- Give it a moment to process the pause or it might return state 3 aka channel is buffering
                    timer.Simple( 0.05, function()
                        expect( channel:GetState() ).to.equal( 2 )
                    end )
                    
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
                    timer.Simple( 0.05, function()
                        expect( channel:GetState() ).to.equal( 2 )
                    end )

                    channel:Pause()
                    timer.Simple( 0.05, function()
                        expect( channel:GetState() ).to.equal( 2 )
                    end )
                    
                    done()
                end )
            end
        },
        {
            name = "Calling pause on a stopped channel is safe and remains stopped",
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