return {
    groupName = "net.ReadSeek",
    cases = {
        {
            name = "Function exists on the built-in net table",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.ReadSeek ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exist",
            when = not HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.ReadSeek ).to.beA( "nil" )
            end
        },
        {
            name = "Errors when called without a position",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.ReadSeek ).to.errWith( "bad argument #1 to '?' (number expected, got no value)" )
            end
        },
        {
            name = "Errors when given a non number position",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.ReadSeek, "string" ).to.errWith( "bad argument #1 to '?' (number expected, got string)" )
            end
        },
        {
            name = "Errors when given a negative position",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.ReadSeek, -1 ).to.errWith( "bad argument #1 to '?' (Number is not allowed to be below 0!)" )
            end
        },
        {
            name = "Errors when there is no active net message being read",
            when = HolyLib_IsModuleEnabled("net"),
            func = function()
                expect( net.ReadSeek, 0 ).to.errWith( "Tried to use net.ReadSeek with no active net message!" )
            end
        },
        {
            name = "Doesn't error when seeking inside an active incoming net message",
            when = HolyLib_IsModuleEnabled("net") and HolyLib_IsModuleEnabled("HolyLib"),
            func = function()
                local bf = bitbuf.CreateWriteBuffer( 64 )
                bf:WriteUBitLong( 0, 8 )

                bf:WriteUBitLong( util.AddNetworkString( "Net_ReadSeek_Test" ), 16 )
                bf:WriteString( "Hello World" )

                local readBF = bitbuf.CreateReadBuffer( bf:GetData() )
                local entity = Entity( 0 )
                local userID = entity:IsPlayer() and entity:UserID() or -1
                local length = readBF:GetNumBits()
                net.Receive( "Net_ReadSeek_Test", function( len, ply )
                    net.ReadSeek( 0 )
                end )

                HolyLib.ReceiveClientMessage( userID, entity, readBF, length )
            end
        },
    }
}
