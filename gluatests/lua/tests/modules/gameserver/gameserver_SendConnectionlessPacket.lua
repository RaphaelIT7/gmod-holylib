return {
    groupName = "gameserver.SendConnectionlessPacket",
    cases = {
        {
            name = "Function exists globally",
            when = HolyLib_IsModuleEnabled( "gameserver" ),
            func = function()
                expect( GetGlobalEntityList ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exists globally",
            when = not HolyLib_IsModuleEnabled( "gameserver" ),
            func = function()
                expect( GetGlobalEntityList ).to.beA( "nil" )
            end
        },
        {
            name = "Sends out loopback packages properly",
            when = HolyLib_IsModuleEnabled( "gameserver" ) && HolyLib_IsModuleEnabled( "bitbuf" ),
            async = true,
            timeout = 1,
            func = function()
                hook.Add( "HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function( bf, ip )
                    expect( ip ).to.equal( "loopback" )
                    expect( bf:ReadString() ).to.equal( "Hello World" )

                    done()
                    return true
                end )

                local bf = bitbuf.CreateWriteBuffer( 64 )
                bf:WriteLong( -1 )
                bf:WriteString( "Hello World" )

                gameserver.SendConnectionlessPacket( bf, "loopback:" .. gameserver.GetUDPPort(), false, 0 )
            end
        },
    }
}
