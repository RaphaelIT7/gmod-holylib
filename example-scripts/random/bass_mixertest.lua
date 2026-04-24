G_mixer = bass.CreateMixerChannel(44100, 2, 0)
local encode, errMsg = G_mixer:CreateEncoder("ogg", 0)
print(encode:ServerInit("55555/test2", 256000 * 8, 256000, 0))
g_Encoder = encode

bass.PlayFile("data/tears.wav", "decode", function(channel, a, b)
    g_Channel = channel
    G_mixer:AddMixerChannel(channel, 0)
end)

bass.PlayFile("data/rush.wav", "decode", function(channel, a, b)
    g_Channel2 = channel
    print(G_mixer:AddMixerChannel(channel, 0))
end)

G_mixer:Play()