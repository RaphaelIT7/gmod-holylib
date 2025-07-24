local BinaryFormat = package.cpath:match("%p[\\|/]?%p(%a+)")
if BinaryFormat == "dll" then
	function os.name()
		return "Windows"
	end
elseif BinaryFormat == "so" then
	function os.name()
		return "Linux"
	end
elseif BinaryFormat == "dylib" then
	function os.name()
		return "MacOS"
	end
end
BinaryFormat = nil

function IsFile(dir)
	return string.find(dir, ".", 1, true)
end

function EndsWith(str, find)
	return str:sub(#str - #find + 1) == find
end

function RemoveEnd(str, find)
	return str:sub(0, #str - #find)
end

function ReadFile(path)
	local file = io.open(path, "rb")
	local content = file:read("*a")
	file:close()
	return content
end

function WriteFile(path, content)
	local file = io.open(path, "wb")
	if file then
		local content = file:write(content)
		file:close()
	end
end

function ScanDir(directory, recursive) -- NOTE: Recursive is super slow!
	local i, t, popen = 0, {}, io.popen
	local pfile
	if os.name() == "Windows" then
		pfile = popen('dir "'..directory..'" /b /a') -- Windows
	else
		pfile = popen('ls "'..directory..'"') -- Linux
	end

	for filename in pfile:lines() do
		i = i + 1
		local isfile = IsFile(filename)
		if not isfile and recursive then
			t[filename] = ScanDir(directory .. "/" .. filename, recursive)
		else
			t[i] = filename
		end
	end
	pfile:close()
	return t
end

local pattern_escape_replacements = { -- Gmod my <3
	["("] = "%(",
	[")"] = "%)",
	["."] = "%.",
	["%"] = "%%",
	["+"] = "%+",
	["-"] = "%-",
	["*"] = "%*",
	["?"] = "%?",
	["["] = "%[",
	["]"] = "%]",
	["^"] = "%^",
	["$"] = "%$",
	["\0"] = "%z"
}
function string.PatternSafe(str)
	return string.gsub(str, ".", pattern_escape_replacements)
end

function string.Trim(s, char)
	if char then
		char = string.PatternSafe(char)
	else
		char = "%s"
	end

	return string.match(s, "^" .. char .. "*(.-)" .. char .. "*$") or s
end