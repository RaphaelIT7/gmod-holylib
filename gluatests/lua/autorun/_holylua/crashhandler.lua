-- Required since HolyLib calls hook.Run for hooks!
hook = { -- Very minimal hook library
	registered = {},
	Add = function(name, func)
		hook.registered[name] = func
	end,
	Run = function(name, ...)
		if hook.registered[name] then
			return hook.registered[name](...)
		end
	end,
}

hook.Add("HolyLib:DetermineServerCrash", function()
	if IsRunningTest then
		return true -- Test execution can easily trigger the watcher- so we tell it here to not mind it hanging!
	end
end)