return {
    groupName = "luagc.GetReferences",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                expect( luagc.GetReferences ).to.beA( "function" )
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
                local refs = luagc.GetReferences()

                expect( refs ).to.beA( "table" )
                expect( next( refs ) ).to.beNil()
            end
        },
        {
            name = "Returns an empty table for a non GC value",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local refs = luagc.GetReferences( 5 )

                expect( refs ).to.beA( "table" )
                expect( next( refs ) ).to.beNil()
            end
        },
        {
            name = "Finds a table that stores the object in its array part",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local target = {}
                local holder = {target}

                local refs = luagc.GetReferences( target )

                local found = false
                for _, obj in pairs( refs ) do
                    if obj == holder then
                        found = true
                        break
                    end
                end

                expect( found ).to.beTrue()
            end
        },
        {
            name = "Finds a table that stores the object as a hash value",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local target = {}
                local holder = {}
                holder.key = target

                local refs = luagc.GetReferences( target )

                local found = false
                for _, obj in pairs( refs ) do
                    if obj == holder then
                        found = true
                        break
                    end
                end

                expect( found ).to.beTrue()
            end
        },
        {
            name = "Finds a table that uses the object as its metatable",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local target = {}
                local holder = setmetatable( {}, target )

                local refs = luagc.GetReferences( target )

                local found = false
                for _, obj in pairs( refs ) do
                    if obj == holder then
                        found = true
                        break
                    end
                end

                expect( found ).to.beTrue()
            end
        },
        {
            name = "Performance",
            when = HolyLib_IsModuleEnabled("luagc"),
            func = function()
                local target = {}
                local holder = {target}
                HolyLib_RunPerformanceTest("luagc.GetReferences", function() luagc.GetReferences(target) end)
            end
        },
    }
}
