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

-- FileExists does not exist! Use this instead and check for nil return!
function ReadFile(path)
	local file = io.open(path, "rb")
	if not file then return end

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

function string.Replace(str, rep, new)
    local new_str = str
    local last = 0
    for k=1, 10 do
        local found, finish = string.find(new_str, rep, last, true)
        if found then
            new_str = string.sub(new_str, 1, found - 1) .. new .. string.sub(new_str, finish + 1)
            last = found + string.len(new)
        end
    end

    return new_str
end

local created_dirs = {}
function CreateDir(name)
	if created_dirs[name] then return end -- To optimize especially on windows.
	if os.name() == "Windows" then
		os.execute('mkdir "' .. string.Replace(name, "/", [[\]]) .. '"')
	else
		os.execute('mkdir -p "' .. name .. '"')
	end

	created_dirs[name] = true
end

function RemoveDir(name)
	if created_dirs[name] then return end -- To optimize especially on windows.
	if os.name() == "Windows" then
		os.execute('rmdir /s /q "' .. string.Replace(name, "/", [[\]]) .. '"')
	else
		if string.len(name) <= 3 then
			error("too short folder name!")
		end

		if name == "." or name == "~/" or name == "../" or name == "/" then
			error("tf are you doing!")
		end

		os.execute('rm -rf "' .. name .. '"')
	end

	created_dirs[name] = nil
end

function CopyFile(from, to)
	CreateDir(GetPath(to))
	WriteFile(to, ReadFile(from))
end

function PrintTable(tbl, indent)
	indent = indent or 0
	local str = ""
	for k=1, indent do
		str = str .. "	"
	end

	for k, v in pairs(tbl) do
		if type(v) == "table" then
			print(str .. k .. " = {")
			PrintTable(v, indent + 1)
			print(str .. "}")
		else
			if type(v) == "string" then
				print(str .. k .. ' = "' .. v .. '"')
			else
				print(str .. k .. " = " .. tostring(v))
			end
		end
	end
end