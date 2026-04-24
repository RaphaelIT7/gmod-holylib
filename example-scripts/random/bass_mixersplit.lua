function SaveBassChannels(channel)
	local leftMixer = bass.CreateMixerChannel(channel:GetSamplingRate(), 1, bass.BASS_STREAM_DECODE)
	local rightMixer = bass.CreateMixerChannel(channel:GetSamplingRate(), 1, 0)

	local leftChannel = bass.CreateSplitChannel(channel, bass.BASS_STREAM_DECODE)
	local rightChannel = bass.CreateSplitChannel(channel, bass.BASS_STREAM_DECODE)

	local leftMatrix = {1.0, 0.0}
	leftChannel:SetMixerMatrix(leftMatrix)

	local rightMatrix = {0.0, 1.0}
	rightChannel:SetMixerMatrix(rightMatrix)

	leftMixer:AddMixerChannel(leftChannel)
	rightMixer:AddMixerChannel(rightChannel)
	
    print("State: " .. leftMixer:GetLength(), leftMixer:GetTime())
	leftMixer:WriteToDisk("__left.ogg", 0, function() print("Wrote left channel") end, true)
	rightMixer:WriteToDisk("__right.ogg", 0, function() print("Wrote right channel") end, true)
end

bass.PlayFile("data/tears.wav", "decode", function(channel, a, b)
	SaveBassChannels(channel)
end)