return {
    groupName = "GetGlobalEntityList",
    cases = {
        --[[ Function Signature ]]--
        --#region
        {
            name = "Function exists globally",
            when = IS_BASE_BRANCH,
            func = function()
                expect( GetGlobalEntityList ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exists globally",
            when = IS_64X_BRANCH,
            func = function()
                expect( GetGlobalEntityList ).to.beA( "nil" )
            end
        },
        {
            name = "Returns an Table object",
            when = IS_BASE_BRANCH,
            func = function()
                local entities = GetGlobalEntityList()
                expect( entities ).to.beA( "table" )
            end
        },
    }
}
