return {
    groupName = "IGModAudioChannel:SetVolume",
    cases = {
        {
            name = "Function exists when module enabled",
            when = HolyLib_IsModuleEnabled( "bass" ),
            func = function()
                expect( FindMetaTable( "IGModAudioChannel" ).SetVolume ).to.beA( "function" )
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
            name = "Setting volume to 0.5 works correctly",
            when = HolyLib_IsModuleEnabled( "bass" ),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( channel ).to.beValid()

                    channel:SetVolume( 0.5 )
                    expect( channel:GetVolume() ).to.equal( 0.5 )
                    
                    done()
                end)
            end
        },
        {
            name = "Setting volume to an negative value is being handled correctly",
            when = HolyLib_IsModuleEnabled("bass"),
            async = true,
            timeout = 2,
            func = function()
                local filePath = "sound/bass_testsound.wav"
                local flags = ""
        
                bass.PlayFile( filePath, flags, function( channel, errorCode, errorMsg )
                    expect( channel ).toNot.beNil()
                    expect( channel ).to.beValid()

                    expect( channel:GetVolume() ).to.equal( 1 )
                    channel:SetVolume( -1 )
                    expect( channel:GetVolume() ).to.equal( 1 )
                    
                    done()
                end)
            end
        },
    }
}