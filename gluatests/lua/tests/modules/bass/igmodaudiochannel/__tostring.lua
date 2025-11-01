return {
    groupName = "IGModAudioChannel:__tostring",
    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ).__tostring ).to.beA( "function" )
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
            name = "__tostring on valid channel returns correct representation",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    local output = channel:__tostring()
                    expect( output ).to.beA( "string" ) 
                    expect( output ).to.equal( "IGModAudioChannel [sound/bass_testsound.wav]" )
                    
                    done()
                end )
            end
        },
        {
            name = "__tostring on invalid channel rerturns IGModAudioChannel [NULL]",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""

                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )       
                    channel:Destroy()
                    local output = channel:__tostring()

                    expect( output ).to.beA( "string" ) 
                    expect( output ).to.equal( "IGModAudioChannel [NULL]" )
                    
                    done()
                end )
            end
        },
    }
}