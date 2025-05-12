return {
    groupName = "steamworks crash",
    cases = {
        {
            name = "Won't crash after shutdown",
            when = HolyLib_IsModuleEnabled( "steamworks" ) && BRANCH == "dev",
            async = true,
            timeout = 1,
            func = function()
                steamworks.Shutdown()
                steamworks.FileInfo(159197754, PrintTable)
            end
        },
    }
}
