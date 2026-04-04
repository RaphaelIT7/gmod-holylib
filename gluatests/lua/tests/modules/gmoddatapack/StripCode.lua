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
	print("server")-- NOTE: if CLIENT isn't supported yet
end

if not CLIENT then
	print("server")-- This does work
else
	print("client")
end

if not CLIENT or SERVER then -- should be valid
	print("server")-- This does work
else
	print("client")
end

if not CLIENT or xyz then-- Should fail due to xyz
	print("server")
else
	print("client")
end

if (not CLIENT) or (SERVER) then-- Should work too
	print("server")
else
	print("client")
end

if CLIENT then
	print("server")-- This does work
elseif SERVER then
	print("server")
end

if CLIENT then
	print("server")-- This does work
elseif SERVER then
	print("server")
elseif xyz then
	print("shared")
end

if CLIENT then
	print("server")-- This does work
elseif SERVER then
	print("server")
else
	print("shared")
end

function test()
	print("hello")
	if !SERVER then return end
	print("world")
end]===]

local resultCode = [===[























do
	print("client")
end

if CLIENT then
	print("client")
else
	print("server")
end



do
	print("client")
end



do
	print("client")
end

if not CLIENT or xyz then
	print("server")
else
	print("client")
end



do
	print("client")
end

if CLIENT then
	print("server")


end

if CLIENT then
	print("server")


elseif xyz then
	print("shared")
end

if CLIENT then
	print("server")


else
	print("shared")
end

function test()
	print("hello")
	if !SERVER then return end
	print("world")
end]===]

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
            name = "Properly strips out comments and server code",
            when = HolyLib_IsModuleEnabled("gmoddatapack"),
            func = function()
                expect( gmoddatapack.StripCode(testCode, true, true) ).to.equal( resultCode )
            end
        },
    }
}
