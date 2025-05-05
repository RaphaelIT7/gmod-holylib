return {
    groupName = "HolyLib.ReceiveClientMessage",
    cases = {
        {
            name = "Function exists in HolyLib table",
            when = HolyLib_IsModuleEnabled( "HolyLib" ),
            func = function()
                expect( HolyLib.ReceiveClientMessage ).to.beA( "function" )
            end
        },
        {
            name = "HolyLib table doesn't exist",
            when = not HolyLib_IsModuleEnabled( "HolyLib" ),
            func = function()
                expect( HolyLib ).to.beA( "nil" )
            end
        },
        {
            name = "Properly fakes a net message",
            when = HolyLib_IsModuleEnabled( "HolyLib" ),
            func = function()
                local bf = bitbuf.CreateWriteBuffer( 64 )
                bf:WriteUBitLong( 0, 8 ) -- The message type. 0 = Lua net message

                bf:WriteUBitLong( util.AddNetworkString( "Example" ), 16 ) -- Header for net.ReadHeader
                bf:WriteString( "Hello World" ) -- Message content

                local readBF = bitbuf.CreateReadBuffer( bf:GetData() ) -- Make it a read buffer.
                local entity = Entity( 0 ) -- We can use the world but normally we shouldn't.
                local userID = entity:IsPlayer() and entity:UserID() or -1
                local length = readBF:GetNumBits() -- Length in bits
                net.Receive( "Example", function( len, ply )
                    expect( net.ReadString() ).to.equal( "Hello World" )
                    expect( len ).to.equal( length - 8 - 16 ) -- -8 because of the message type, -16 because of net.ReadHeader
                    expect( ply ).to.equal( entity )
                end )

                HolyLib.ReceiveClientMessage( userID, entity, readBF, length )
            end
        },
    }
}
