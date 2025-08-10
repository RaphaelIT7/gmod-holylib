# Example Module DLL
This is an example DLL that is loaded by holylib, and it registers its own module.

## EntryPoint
First, you'll need an entry point, this will look like this:
```cpp
extern "C" bool HolyLib_EntryPoint(IHolyLib* pHolyLib)
{
	// Your stuff

	return true; // if we return false, HolyLib will throw a warning that we failed to load and will unload us again.
}
```

After this, you'll need to add your module to HolyLib's config.
Open the `garrysmod/holylib/cfg/dlls.json`, if it doesn't exist, create it and fill it with these contents:
```json
[
	{
		"path": "garrysmod/lua/bin/gmsv_example_linux.dll", // Must be relative to the garrysmod/ directory
		"name": "ExampleDLL", // Name that will be used for debug prints.
		"version": "0.8", // (Optional) a specific version of holylib it might be bound to, the DLL won't be loaded if these don't match.
		"branch": "main", // (Optional) a specific branch of holylib
	},
]
```

If everything went fine, on your next startup you should see `HolyLib - DLLManager: Successfully loaded "ExampleDLL"` in your console.

## IHolyLib
This is an interface provided by HolyLib, which you can find here: https://github.com/RaphaelIT7/gmod-holylib/blob/main/source/public/iholylib.h<br>
Using this interface, you can for example, register your own module to HolyLib.

> [!WARNING]
> If you use dev builds of HolyLib, interfaces will most likely change, ensure that your DLLs either use the newest interface of HolyLib, or 