if CLIENT then return end

local REQUEST_CHANNEL = string.byte("z")
function BuildNetChannel(target, status) -- status should not be set when called
    local bf = bitbuf.CreateWriteBuffer(64)

    bf:WriteLong(-1) -- CONNECTIONLESS_HEADER
    bf:WriteByte(REQUEST_CHANNEL) -- Our header
    bf:WriteByte(status or 0) -- 0 = We requested it.

    gameserver.SendConnectionlessPacket(bf, target)
end

function IncomingNetMessage(channel, bf, length)
    print("Received a message at " .. tostring(channel), bf, length)
    local header = bf:ReadUBitLong(4)
    if header == 1 then
        print("Received message:" .. bf:ReadString())
    end
end

netChannels = netChannels or {}
hook.Add("HolyLib:ProcessConnectionlessPacket", "ProcessResponse", function(bf, ip)
    local header = bf:ReadByte()
    if header != REQUEST_CHANNEL then return end

    local status = bf:ReadByte()

    local netChannel = gameserver.CreateNetChannel(ip)
    netChannel:SetMessageCallback(function(bf, length)
        IncomingNetMessage(netChannel, bf, length)
    end)
    table.insert(netChannels, netChannel)

    if status == 0 then
        print("Created a requested net channel to " .. ip)

        BuildNetChannel(ip, 1) -- Respond to the sender to confirm creation.
    elseif status == 1 then
        print("Created our net channel to " .. ip)
    end

    return true
end)

function SendNetMessage(target, bf, reliable)
    for _, channel in ipairs(netChannels) do
        if not channel:IsValid() then continue end
        if channel:GetName() != target then continue end

        return channel:SendMessage(bf, reliable)
    end

    return false
end

hook.Add("Think", "UpdateNetChannels", function()
    for _, channel in ipairs(netChannels) do
        if not channel:IsValid() then continue end

        channel:ProcessStream() -- process any incoming messages
        channel:Transmit() -- Transmit out a update.
    end
end)

function SendNetPrint(target, message)
    for _, channel in ipairs(netChannels) do
        if not channel:IsValid() then continue end
        if channel:GetName() != target then continue end

        local bf = bitbuf.CreateWriteBuffer(100, message)
        bf:WriteUBitLong(1, 4)
        bf:WriteString(message)

        return channel:SendMessage(bf, false)
    end

    return false
end
