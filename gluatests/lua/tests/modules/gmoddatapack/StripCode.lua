local testCode = [===[
-- Example Comment

--[[
	Another Comment	
]]

--[=[
	--[[
		Comment in a Comment	
	]]
]=]

/*
	C comment
*/

// Another C comment

if SERVER then
	-- REMOVEME
end

if SERVER then
else
	print("client")
end

if CLIENT then
	print("client")
else
	print("server") -- NOTE: if CLIENT isn't supported yet
end
]===]

local resultCode = [===[























do
	print("client")
end

if CLIENT then
	print("client")
else
	print("server")
end
]===]

return {
    groupName = "gmoddatapack.StripCode",
    cases = {
        {
            name = "Function exists on table",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.StripCode ).to.beA( "function" )
            end
        },
        {
            name = "Table doesn't exist",
            when = not HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack ).to.beA( "nil" )
            end
        },
        {
            name = "Properly creates a buffer from a string",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.StripCode(testCode, true, true) ).to.equal( resultCode )
            end
        },
    }
}
