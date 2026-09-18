return {
    groupName = "luagc.GetSizeOfGCObject",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetSizeOfGCObject ).to.beA( "function" )
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
            name = "Returns 0 when called without arguments",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetSizeOfGCObject() ).to.equal( 0 )
            end
        },
        {
            name = "Returns 0 for a non GC value",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetSizeOfGCObject( 5 ) ).to.equal( 0 )
            end
        },
        {
            name = "Returns a positive size for a real GC object",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local obj = {}

                expect( luagc.GetSizeOfGCObject( obj ) ).to.beGreaterThan( 0 )
            end
        },
        {
            name = "Recursive sizing also includes grandchildren, unlike a flat size",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local grandchild = {1, 2, 3, 4, 5}
                local child = {grandchild}
                local parent = {child}

                local flatSize = luagc.GetSizeOfGCObject( parent, false )
                local recursiveSize = luagc.GetSizeOfGCObject( parent, true )

                expect( recursiveSize > flatSize ).to.beTrue()
            end
        },
        {
            name = "ignoreGCObjects excludes the given object's contribution from a recursive size",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local child = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
                local parent = {child}

                local recursiveSize = luagc.GetSizeOfGCObject( parent, true )
                local ignoredSize = luagc.GetSizeOfGCObject( parent, true, {child} )

                expect( ignoredSize < recursiveSize ).to.beTrue()
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local obj = {1, 2, 3}
                HolyLib_RunPerformanceTest("luagc.GetSizeOfGCObject", function() luagc.GetSizeOfGCObject(obj) end)
            end
        },
    }
}
