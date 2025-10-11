return {
    groupName = "FileSystem:AbsoluteSearchCache",
    cases = {
        {
            name = "Searchpaths won't mix when enabled.",
            when = HolyLib_IsModuleEnabled( "filesystem" ),
            func = function(state)
                expect( file.Exists("test.folder/placeholder.txt", "MOD") ).to.beFalse()

                expect( file.Exists("test.folder/placeholder.txt", "thirdparty") ).to.beTrue() -- Since it will find the file, it will add it into the cache now.

                expect( file.Exists("test.folder/placeholder.txt", "MOD") ).to.beFalse()
            end,
        },
    }
}
