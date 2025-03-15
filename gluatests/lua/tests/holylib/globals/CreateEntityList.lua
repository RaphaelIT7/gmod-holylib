return {
    groupName = "CreateEntityList",
    cases = {
        --[[ Function Signature ]]--
        --#region
        {
            name = "Function exists globally",
            when = IS_BASE_BRANCH,
            func = function()
                expect( CreateEntityList ).to.beA( "function" )
            end
        },
        {
            name = "Function doesn't exists globally",
            when = IS_64X_BRANCH,
            func = function()
                expect( CreateEntityList ).to.beA( "nil" )
            end
        },
        {
            name = "Returns an Table object",
            when = IS_BASE_BRANCH,
            func = function()
                local list = CreateEntityList()
                expect( list ).to.beA( "EntityList" )
            end
        },
    }
}
