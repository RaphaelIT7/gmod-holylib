do
	local ffi = jit.getffi and jit.getffi() or require("ffi")

	ffi.cdef([[
		typedef struct {
			int iPlayerSlot;
			bool bProximity;
			bool bDecompressedChanged;
			bool bAllowLuaGC;

			int iLength;
			int iDecompressedLength;

			char* pData;
			char* pDecompressedData;
		} HOLYLIB_VoiceData;
	]])
end

local function CastUserDataToVoiceData(userdata)
	if userdata.data == 0 then
		return nil
	end

	return ffi.cast("HOLYLIB_VoiceData*", userdata.data)
end

local voiceDataMeta = FindMetaTable("VoiceData")
local methods = {}
local mt = {
	__index = function(self, key)
		local method = methods[key]
		if method then
			return method
		end

		return voiceDataMeta.__index(self, key)
	end,
	__newindex = function(self, key, value)
		return voiceDataMeta.__newindex(self, key, value)
	end,
	__tostring = function(self)
		return voiceDataMeta.__tostring(self)
	end,
	MetaName = voiceDataMeta.MetaName,
	MetaID = voiceDataMeta.MetaID,
}

-- We do this so that we also have things like VoiceData():__tostring() working
for name, func in pairs(mt) do
	if isfunction(func) then
		methods[name] = func
	end
end

function mt.GetPlayerSlot(userdata)
	local voiceData = CastUserDataToVoiceData(userdata)
	if not voiceData then -- Not our problem
		return voiceDataMeta.GetPlayerSlot(userdata)
	end

	return voiceData.iPlayerSlot
end

__HOLYLIB_FFI.RegisterOverride(mt)