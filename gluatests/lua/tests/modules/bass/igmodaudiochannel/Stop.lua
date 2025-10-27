return {
    groupName = "IGModAudioChannel:Stop",
    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable("IGModAudioChannel").Stop ).to.beA( "function" )
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
            name = "Stops playing the audio channel",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( channel:GetState() ).to.equal( 1 )

                    channel:Stop()
                    timer.Simple( 0.05, function()
                        expect( channel:GetState() ).to.equal( 0 )
                    end )
                    
                    done()
                end )
            end
        },
        {
            name = "Calling Stop() twice is safe",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "noplay"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()

                    channel:Stop()
                    timer.Simple( 0.05, function()
                        expect( channel:GetState() ).to.equal( 0 )
                    end )
                    
                    done()
                end )
            end
        },
        {
            name = "Audio channel remains valid after calling stop()",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()

                    channel:Stop()
                    expect( channel ).to.beValid()
                    
                    done()
                end )
            end
        },
        {
            name = "Stopped Audio channel can only be resumed with Play()",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( channel:GetState() ).to.equal( 1 )

                    channel:Stop()
                    timer.Simple( 0.05, function()
                        expect( channel:GetState() ).to.equal( 0 )
                    end )

                    channel:Pause()
                    timer.Simple( 0.05, function()
                        expect( channel:GetState() ).to.equal( 0 )
                    end )

                    channel:Play()
                    timer.Simple( 0.05, function()
                        expect( channel:GetState() ).to.equal( 1 )
                    end )
                    
                    done()
                end )
            end
        },
    }
}