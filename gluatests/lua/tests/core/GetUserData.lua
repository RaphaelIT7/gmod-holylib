return {
    groupName = "HolyLib manages to properly Get HolyLib UserData to Lua",
    cases = {
        {
            name = "Is _HOLYLIB_CORE.GetTestUserData function existent",
            func = function()
                expect( _HOLYLIB_CORE.GetTestUserData ).to.beA( "function" )
            end
        },
        {
            name = "Properly got the valid pointer to our userdata",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                expect( userdata ).to.beA( "_HOLYLIB_CORE_TEST" )

                -- If we got a garbage pointer, this will return false or crash.
                expect( _HOLYLIB_CORE.GetTestUserData(userdata) ).to.beTrue()
            end
        },
        {
            name = "__newindex and __index functions properly work on userdata",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                expect( userdata ).to.beA( "_HOLYLIB_CORE_TEST" )

                userdata.example = "Hello World"

                expect( userdata.example ).to.equal( "Hello World" )
            end
        },
        {
            name = "Performance",
            func = function()
                local userdata = _HOLYLIB_CORE.PushTestUserData()
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.GetTestUserData", _HOLYLIB_CORE.RawGetTestUserData, userdata)

                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.__newindex", function(userdata) userdata.example = "Hello World" end, userdata)
                HolyLib_RunPerformanceTest("_HOLYLIB_CORE.__index", function(userdata) return userdata.example end, userdata)
            end
        },
        {
            name = "GMod Performance",
            func = function()
                local ent = game.GetWorld()
                local tab = ent:GetTable()
                local prevEnv = debug.getfenv(ent)

                HolyLib_RunPerformanceTest("(GMOD) Entity.__newindex", function(ent) ent.example = "Hello World" end, ent)
                HolyLib_RunPerformanceTest("(GMOD) Entity.__index", function(ent) return ent.example end, ent)

                if debug.userdata_setusertable and debug.userdata_setmetaaccess then
                    debug.userdata_setusertable(ent, true) -- IMPORTANT: This creates and sets an empty table into GCudata::env so do NOT use debug.fenv before this!
                    debug.userdata_setmetaaccess(ent, true)
                end

                debug.setfenv(ent, tab) -- we must fill the GCudata::env field

                HolyLib_RunPerformanceTest("(HolyLib) Entity.__newindex", function(ent) ent.example = "Hello World" end, ent)
                HolyLib_RunPerformanceTest("(HolyLib) Entity.__index", function(ent) return ent.example end, ent)

                -- BUG: IDK why but JIT dies :sob:
                if debug.userdata_setusertable and debug.userdata_setmetaaccess then
                    debug.userdata_setusertable(ent, false)
                    debug.userdata_setmetaaccess(ent, false)
                end

                debug.setfenv(ent, prevEnv) -- Restore original since the GC does not like in GCudata::env (In default JIT it would crash! but fixed in our version)

                -- Let's try to flush out those broken traces as GMod's userdata really hates these flags.
                -- They are used for HolyLib's userdata and work just fine there- but try it on GMod's? GGs
                jit.flush()
                for k=1, 10 do
               	    collectgarbage("collect")
               	end
            end
        },
    }
}
