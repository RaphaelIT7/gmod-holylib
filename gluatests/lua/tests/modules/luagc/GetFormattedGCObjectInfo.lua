return {
    groupName = "luagc.GetFormattedGCObjectInfo",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetFormattedGCObjectInfo ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc ).to.beA( "nil" )
            end
        },
        {
            name = "Returns nil when called without arguments",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetFormattedGCObjectInfo() ).to.beNil()
            end
        },
        {
            name = "Returns nil for a non GC value",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetFormattedGCObjectInfo( 5 ) ).to.beNil()
            end
        },
        {
            name = "Describes a table's metatable, array part and hash part",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local mt = {}
                local child = {}
                local obj = setmetatable( {child}, mt )
                obj.key = "value"

                local info = luagc.GetFormattedGCObjectInfo( obj )

                expect( info ).to.beA( "table" )
                expect( info.type ).to.equal( "table" )
                expect( info.object ).to.equal( obj )
                expect( info.metatable ).to.equal( mt )
                expect( info.size ).to.beGreaterThan( 0 )
                expect( info.arraySlots[1] ).to.equal( child )
                expect( info.hashSlots[1].key ).to.equal( "key" )
                expect( info.hashSlots[1].value ).to.equal( "value" )
            end
        },
        {
            name = "Describes a table without a metatable, array part or hash part",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local obj = {}

                local info = luagc.GetFormattedGCObjectInfo( obj )

                expect( info.type ).to.equal( "table" )
                expect( info.object ).to.equal( obj )
                expect( info.metatable ).to.beNil()
                expect( info.arraySlots ).to.beNil()
                expect( info.hashSlots ).to.beNil()
            end
        },
        {
            name = "Describes a function's environment, proto and upvalues",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local env = {marker = true}
                local upvalueValue = {}
                local function testFn()
                    return upvalueValue
                end
                debug.setfenv( testFn, env )

                local info = luagc.GetFormattedGCObjectInfo( testFn )

                expect( info.type ).to.equal( "function" )
                expect( info.object ).to.equal( testFn )
                expect( info.environment ).to.equal( env )
                expect( type( info.proto ) ).to.equal( "proto" )

                expect( type( info.upvalues[1] ) ).to.equal( "upval" )

                local upvalueInfo = luagc.GetFormattedGCObjectInfo( info.upvalues[1] )
                expect( upvalueInfo.value ).to.equal( upvalueValue )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local obj = {1, 2, 3}
                HolyLib_RunPerformanceTest("luagc.GetFormattedGCObjectInfo", function() luagc.GetFormattedGCObjectInfo(obj) end)
            end
        },
    }
}
