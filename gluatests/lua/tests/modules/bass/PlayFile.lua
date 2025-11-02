return {
    groupName = "bass.PlayFile",

    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( bass.PlayFile ).to.beA( "function" )
            end
        },
        {
            name = "Function is nil when module disabled",
            when = not HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( bass ).to.beA( "nil" )
            end
        },
        {
            name = "Playing valid sample audio file with mono flag",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "mono"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( errorCode ).to.equal( 0 )
                    expect( errorMsg ).to.beNil()

                    expect( channel:GetState() ).to.equal( 1 )
                    
                    done()
                end )
            end
        },
        {
            name = "Playing valid sample audio file with default flags",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( errorCode ).to.equal( 0 )
                    expect( errorMsg ).to.beNil()

                    expect( channel:GetState() ).to.equal( 1 )
                    
                    done()
                end )
            end
        },
        {
            name = "Playing valid sample audio file with noplay flag",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "noplay"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( errorCode ).to.equal( 0 )
                    expect( errorMsg ).to.beNil()

                    expect( channel:GetState() ).to.equal( 0 )
                    
                    done()
                end )
            end
        },
        {
            name = "Playing valid sample audio file with noblock flag",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "noblock"
        
                bass.PlayFile(filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( errorCode ).to.equal( 0 )
                    expect( errorMsg ).to.beNil()

                    expect( channel:GetState() ).to.equal( 1 )
                    
                    done()
                end )
            end
        },
        {
            name = "Ingore unknown or non-sensical flags",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "holy noplay hello"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( errorCode ).to.equal( 0 )
                    expect( errorMsg ).to.beNil()

                    expect( channel:GetState() ).to.equal( 0 )
                    
                    done()
                end )
            end
        },
        {
            name = "Correctly handles 3D audio flag",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = "3d mono"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( errorCode ).to.equal( 0 )
                    expect( errorMsg ).to.beNil()

                    expect( channel:GetState() ).to.equal( 1 )
                    
                    done()
                end )
            end
        },
        -- Test error handling based on BASS error codes
        -- https://www.un4seen.com/doc/#bass/BASS_ErrorGetCode.html
        {
            name = "Handles invalid file paths gracefully",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/thisFileIsCool.wav"
                local flags = "mono"
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).to.beNil()
                    expect( errorCode ).to.equal( 2 )
                    expect( errorMsg ).to.equal( "BASS_ERROR_FILEOPEN" )
                    
                    done()
                end )
            end
        },
        {
            name = "Handles non-audio files gracefully",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/not_real.txt"
                local flags = ""

                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).to.beNil()
                    expect( errorCode ).to.equal( 41 )
                    expect( errorMsg ).to.equal( "BASS_ERROR_FILEFORM" )
                    
                    done()
                end )
            end
        },
    }
}