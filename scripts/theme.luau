-- This is the type system for the Theme API as it exists in C++ code.

local gui = require("@Luwow/gui")

--[=[
	How windows are themed. A field left out is the defaults' from theme.configure, and where those leave it out too,
	the platform's own.

	Windows gives its controls the system's visual styles once the library is loaded, and takes every option.
	macOS and Linux keep their own look and only take ColorScheme. On Linux it's the application's, as GTK has one
	for every window, so the one last given is every window's.
]=]
export type Options = {
	-- "System" follows the system's light or dark setting, and changes with it. On Windows, high contrast keeps its
	-- own colors whatever this is.
	ColorScheme: ("System" | "Light" | "Dark")?,
	-- The controls' font, Segoe UI by default. Windows only.
	FontName: string?,
	-- The controls' font size in points, 9 by default. Windows only.
	FontSize: number?,
	-- The window's corners on Windows 11. Earlier versions keep theirs.
	Corners: ("Default" | "Round" | "Small" | "Square")?,
}

local theme = {}

--[=[
	Sets the defaults every window themed takes where its own options leave a field out, theming the windows
	themed again with them. The fields left out keep what they were.

	@param options The fields to change.
]=]
theme.configure = (nil :: any) :: (options: Options) -> ()

--[=[
	Themes a window and its controls, the ones it gets later too. Applying again changes the fields given and keeps
	the others.

	@param window The window, which must be open.
	@param options The window's own options, over the defaults.
]=]
theme.apply = (nil :: any) :: (window: gui.Window, options: Options?) -> ()

export type Theme = typeof(theme)

return table.freeze(theme)
