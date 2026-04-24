function AskForServerInfo(targetIP, challenge)
	local bf = bitbuf.CreateWriteBuffer(64)

	bf:WriteLong(-1) -- CONNECTIONLESS_HEADER
	bf:WriteByte(string.byte("T")) -- A2S_INFO Header
	bf:WriteString("Source Engine Query") -- Null terminated string

	if challenge then
		bf:WriteLong(challenge) -- Challange response if we got a S2C_CHALLENGE
	end

	gameserver.SendConnectionlessPacket(bf, targetIP)
end

function GetServerInfo(targetIP)
	AskForServerInfo(targetIP)

	hook.Add("HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function(bf, ip)
		if ip != targetIP then return end

		local msgHeader = bf:ReadByte()
		if msgHeader == string.byte("A") then -- It responded with a S2C_CHALLENGE
			AskForServerInfo(targetIP, bf:ReadLong())
			return true
		end

		if msgHeader != string.byte("I") then return end -- We didn't receive a A2S_INFO

		local a2s_info = {
			Header = string.char(msgHeader),
			Protocol = bf:ReadByte(),
			Name = bf:ReadString(),
			Map = bf:ReadString(),
			Folder = bf:ReadString(),
			Game = bf:ReadString(),
			ID = bf:ReadShort(),
			Players = bf:ReadByte(),
			MaxPlayers = bf:ReadByte(),
			Bots = bf:ReadByte(),
			ServerType = bf:ReadByte(),
			Environment = string.char(bf:ReadByte()),
			Visibility = bf:ReadByte(),
			VAC = bf:ReadByte(),
			Version = bf:ReadString(),
			ExtraDataFlag = bf:ReadByte(),
		}

		if bit.band(a2s_info.ExtraDataFlag, 0x80) != 0 then
			a2s_info.Port = bf:ReadShort()
		end

		if bit.band(a2s_info.ExtraDataFlag, 0x10) != 0 then
			a2s_info.SteamID = bf:ReadLongLong()
		end

		if bit.band(a2s_info.ExtraDataFlag, 0x40) != 0 then
			a2s_info.SourceTVPort = bf:ReadShort()
			a2s_info.SourceTVName = bf:ReadString()
		end

		if bit.band(a2s_info.ExtraDataFlag, 0x20) != 0 then
			a2s_info.Tags = bf:ReadString()
		end

		if bit.band(a2s_info.ExtraDataFlag, 0x01) != 0 then
			a2s_info.GameID = bf:ReadLongLong()
		end

		PrintTable(a2s_info)
		return true
	end)
end

function AskForServerPlayerInfo(targetIP, challenge)
	local bf = bitbuf.CreateWriteBuffer(64)

	bf:WriteLong(-1) -- CONNECTIONLESS_HEADER
	bf:WriteByte(string.byte("U")) -- A2S_PLAYER Header

	bf:WriteUBitLong(challenge or -1, 32) -- Challange response if we got a S2C_CHALLENGE

	gameserver.SendConnectionlessPacket(bf, targetIP)
end

function GetServerPlayerInfo(targetIP)
	AskForServerPlayerInfo(targetIP)

	hook.Add("HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function(bf, ip)
		if ip != targetIP then return end

		local msgHeader = bf:ReadByte()
		if msgHeader == string.byte("A") then -- It responded with a S2C_CHALLENGE
			AskForServerPlayerInfo(targetIP, bf:ReadUBitLong(32))
			return true
		end

		if msgHeader != string.byte("D") then return end -- We didn't receive a A2S_PLAYER

		local a2s_player = {
			Header = string.char(msgHeader),
			Players = bf:ReadByte(),
		}

		for k=1, a2s_player.Players do
			local entry = {}
			a2s_player[k] = entry

			entry.Index = bf:ReadByte()
			entry.Name = bf:ReadString()
			entry.Score = bf:ReadLong()
			entry.Duration = bf:ReadFloat()
		end

		PrintTable(a2s_player)
		return true
	end)
end

function AskForChallenge(targetIP, challenge)
	local bf = bitbuf.CreateWriteBuffer(64)

	bf:WriteLong(-1) -- CONNECTIONLESS_HEADER
	bf:WriteByte(string.byte("q")) -- A2S_GETCHALLENGE Header
	bf:WriteLong(-1) -- retry challenge
	bf:WriteString("0000000000") -- pad out

	gameserver.SendConnectionlessPacket(bf, targetIP)
end

function RequestConnection(targetIP)
	AskForChallenge(targetIP)

	hook.Add("HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function(bf, ip)
		if ip != targetIP then return end

		local msgHeader = bf:ReadByte()
		if msgHeader != string.byte("A") then return true end -- It didn't responded with a S2C_CHALLENGE

		print(bf, ip, msgHeader)
	end)
end