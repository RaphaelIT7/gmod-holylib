return {
    groupName = "luagc.GetContainingReferences",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetContainingReferences ).to.beA( "function" )
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
            name = "Returns an empty table when called without arguments",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local refs = luagc.GetContainingReferences()

                expect( refs ).to.beA( "table" )
                expect( next( refs ) ).to.beNil()
            end
        },
        {
            name = "The ignore list is passed through into the result even with an invalid object argument",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local ignored = {}

                local refs = luagc.GetContainingReferences( nil, false, {ignored} )

                expect( #refs ).to.equal( 1 )
                expect( refs[1] ).to.equal( ignored )
            end
        },
        {
            name = "Does not crash for a table without a metatable and returns an empty result",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local obj = {}

                local refs = luagc.GetContainingReferences( obj )

                expect( refs ).to.beA( "table" )
                expect( next( refs ) ).to.beNil()
            end
        },
        {
            name = "Returns the direct references of an object without recursing",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local mt = {}
                local obj = setmetatable( {}, mt )

                local refs = luagc.GetContainingReferences( obj )

                expect( #refs ).to.equal( 1 )
                expect( refs[1] ).to.equal( mt )
            end
        },
        {
            name = "Recursively follows references and doesn't loop forever on cycles",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local mt = {}
                setmetatable( mt, mt )

                local obj = setmetatable( {}, mt )

                local refs = luagc.GetContainingReferences( obj, true )

                expect( #refs ).to.equal( 1 )
                expect( refs[1] ).to.equal( mt )
            end
        },
        {
            name = "Ignored objects stop recursion but still appear in the result",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local ignoredChild = {}
                local obj = setmetatable( {}, ignoredChild )

                local refs = luagc.GetContainingReferences( obj, true, {ignoredChild} )

                expect( #refs ).to.equal( 1 )
                expect( refs[1] ).to.equal( ignoredChild )
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local mt = {}
                local obj = setmetatable( {}, mt )
                HolyLib_RunPerformanceTest("luagc.GetContainingReferences", function() luagc.GetContainingReferences(obj) end)
            end
        },
    }
}
