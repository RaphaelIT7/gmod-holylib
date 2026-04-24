g_VoiceChannel = bass.CreatePushChannel(24000, 1, 0) -- 24000 = SAMPLERATE_GMOD_OPUS
-- ToDo: Figure out why mixer channels are also broken, why is bass so weird :sob:
-- Update: They are not, I just forgot to call :Play on them!
-- g_MixerChannel = bass.CreateMixerChannel(24000, 1, 0)
g_Encoder = g_VoiceChannel:CreateEncoder("ogg", 0) -- :sob: opus doesn't work for clients

local bassPort = 55555
print(g_Encoder:ServerInit(bassPort .. "/player1", 8192, 2048, 0))

-- Gotta use a Push channel as else we cannot apply FX as we'd have to insert data directly into the encoder, not the channel
g_VoiceChannel:SetFX("EchoTest", bass.FX_ECHO, 0, {
	fWetDryMix = 50,
	fFeedback = 40,
	fLeftDelay = 500,
	fRightDelay = 500,
	lPanDelay = false,
})
-- g_VoiceChannel:RemoveFX("EchoTest")
-- ToDo: Figure out why :RemoveFX causes a crash :sob:
-- Update: Crash is fixed, it was due to it internally not returning true leading to undefined behavior

local emptyVoiceData = voicechat.CreateVoiceData(0, nil, 0)
hook.Add("HolyLib:PreProcessVoiceChat", "VoiceChat_Over_Bass", function(pClient, voiceData)
	g_Encoder:InsertVoiceData(voiceData) -- Pray for functionality, the prayers worked
	-- ToDo: Figure out why now push streams aren't feeding the encoder, I want my FX >:(
	-- g_VoiceChannel:InsertVoiceData(voiceData)

	-- ToDo: Fix up voiceData, maybe add voiceData:Empty() to clear all data off it so that this isn't needed
	emptyVoiceData:SetPlayerSlot(pClient:EntIndex()-1)
	voicechat.ProcessVoiceData(pClient, emptyVoiceData)

	return true
end)

--- Client code
if SERVER then return end

sound.PlayURL("http://" .. string.Split(game.GetIPAddress(), ":")[1] .. ":" .. bassPort .. "/player1", "3d", function(channel)
	g_Channel = channel -- GC
end)

hook.Add("Think", "", function()
	g_Channel:SetPos(LocalPlayer():EyePos())
end)