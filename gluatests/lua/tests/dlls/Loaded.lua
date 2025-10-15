return {
    groupName = "HolyLib managed to load our example DLL",
    cases = {
        { -- Before anyone hits me, yes we do this in one case since we unload the DLL & having multiple cases could cause more work.
            name = "Test example dll",
            when = util.IsBinaryModuleInstalled("exampledll"),
            func = function()
                expect( example ).to.beA( "table" )
                expect( example.hello ).to.beA( "function" )

                expect( example.hello() ).to.equal( "Hello World" )

                RunConsoleCommand("holylib_enable_example", "0")
                HolyLib.ServerExecute() -- If we error, then holylib itself is fked! This function should be exposed on any platform & branch!

                expect( example ).to.beA( "nil" )

                RunConsoleCommand("holylib_enable_example", "1")
                HolyLib.ServerExecute()

                expect( example ).to.beA( "table" )
                expect( example.hello ).to.beA( "function" )
            end
        },
    }
}