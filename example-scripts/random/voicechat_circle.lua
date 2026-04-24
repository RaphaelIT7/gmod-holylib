local ply = nil
local oldPly = nil
local nextPly = nil
hook.Add("HolyLib:PreProcessVoiceChat", "VoiceChat_Example", function(voicePly, voiceData) 
	if !ply then
		if !nextPly then return end
		ply = nextPly
	end

	if oldPly then
		--voiceData:SetPlayerSlot(oldPly:EntIndex() - 1)
		--voicechat.SendVoiceData(voicePly, voiceData)
		oldPly = nil
	end

	voiceData:SetPlayerSlot(ply:EntIndex() - 1)
	voicechat.SendVoiceData(voicePly, voiceData)

	if nextPly then -- Give the client a voice frame to prepare
		voiceData:SetPlayerSlot(nextPly:EntIndex() - 1)
		voicechat.SendVoiceData(voicePly, voiceData)
		oldPly = ply
		ply = nextPly
		nextPly = nil
	end
end)

local nextSlot = 0
timer.Create("VoiceRotate", 0.1, 0, function()
	local bots = player.GetBots()
	nextSlot = nextSlot + 1
	if nextSlot > #bots then
		nextSlot = 1
	end

	nextPly=bots[nextSlot]
end)

function PlotCirclePoints(points, radius, center)
    local pointstbl = {}
    local slice = math.rad(360 / points)
    for i = 0, points do
        local angle = slice * i
        local newX = center.x + radius * math.cos(angle)
        local newY = center.y + radius * math.sin(angle)
        local point = Vector(newX, newY, center.z)
      
        table.insert(pointstbl, i , point)
    end

    return pointstbl
end

concommand.Add("makecircle", function(ply)
	local bots = player.GetBots()
	local points = PlotCirclePoints(#bots, #bots * 64, ply:GetPos())

	for k, bot in ipairs(bots) do
		bot:SetPos(points[k])
	end
end)