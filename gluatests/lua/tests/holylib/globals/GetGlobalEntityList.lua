return {
    groupName = "GetGlobalEntityList",
    cases = {
        --[[ Function Signature ]]--
        --#region
        {
            name = "Function exists globally",
            func = function()
                expect( GetGlobalEntityList ).to.beA( "function" )
            end
        },
        {
            name = "Returns an Table object",
            func = function()
                local entities = GetGlobalEntityList()
                expect( entities ).to.beA( "table" )
            end
        },
    }
}
