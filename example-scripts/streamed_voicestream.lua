if CLIENT then return end

--[[
	This networks the VoiceData while playing it, from Server A to Server B and is there played from a bot.
	This should probably use a NetChannel between the servers instead, but this way it was easier to do.
	Though this is not that secure / you should verify the IP that is sending the packets to one.
]]

local PROCESS_VOICEDATA = string.byte("K")
hook.Add("HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function(bf, ip)
    local header = bf:ReadByte()
    if header != PROCESS_VOICEDATA then return end

    local bot = player.GetBots()[1]
    if not bot then
        RunConsoleCommand("bot")
        return true
    end

    local length = bf:ReadUBitLong(16)
    local pRawData = bf:ReadBytes(length)

    local pVoiceData = voicechat.CreateVoiceData(bot:EntIndex() - 1, pRawData)
    pVoiceData:SetProximity(false) -- Else it might not be played at all
    voicechat.BroadcastVoiceData(pVoiceData)
    pVoiceData:__gc()

    return true
end)

function SendVoiceData(fileName, targetHost) -- targetHost has to be ip:port
    local voiceStream, status = voicechat.LoadVoiceStream(fileName, "DATA", true, function(voiceStream, status)
        print("Async LoadVoiceStream callback", voiceStream, status)
        if voiceStream then
            local voiceIdx = 0
            local voiceTable = voiceStream:GetData()
            local bf = bitbuf.CreateWriteBuffer(2048)
            hook.Add("Think", "VoiceChat_Transmit:" .. targetHost, function()
                voiceIdx = voiceIdx + 1
                local voiceData = voiceTable[voiceIdx]
                if not voiceData then return end

                bf:Reset()
                bf:WriteLong(-1) -- CONNECTIONLESS_HEADER
                bf:WriteByte(PROCESS_VOICEDATA) -- Our header
                bf:WriteUBitLong(voiceData:GetLength(), 16)
                bf:WriteBytes(voiceData:GetData())

                gameserver.SendConnectionlessPacket(bf, targetHost)
            end)
        end
    end)
end