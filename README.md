# Theme
An extension of the GUI library that themes its windows, for Luwow-Project.

This library is not stand-alone. You must build it externally from within another
repository, alongside the GUI library.

## Project Overview

This project contains the sources for the Luwow Theme library.

It reaches the GUI library's windows through their native handles.

- **Windows**: the controls get the system's visual styles (common controls version 6), the system's UI font,
  and no sunken classic edge. Windows can be dark, follow the system's setting, and take a font and Windows 11
  corners. The library has to be loaded before windows are made, since a control keeps the look it was made with.
  The menu bar stays light in the dark.
- **macOS**: `ColorScheme` sets each window's appearance, light or dark, and
  `"System"` follows the system's as it changes. The other options change nothing.
- **Linux**: GTK 3 has one light or dark setting for the whole application, so
  `ColorScheme` is every window's, the one last given winning. `"Dark"` prefers the theme's dark variant, `"Light"`
  uses the light variant of a theme named like `Yaru-dark`, and `"System"` follows the desktop's theme and GNOME's
  dark style setting. The other options change nothing.

The library only supports the `SERIAL` run mode.

## Library Documentation

### `theme.configure(options: Options)`

Sets the defaults every window themed takes where its own options leave a field out, and themes the windows
already themed again. The fields left out keep what they were.

### `theme.apply(window: Window, options: Options?)`

Themes an open window and its controls, the ones added later too. Applying again changes the fields given.

### `Options`

| Field | Values | Default |
|---|---|---|
| `ColorScheme` | `"System"`, `"Light"`, `"Dark"` | `"System"`, following the system's light or dark setting |
| `FontName` | a font's name, Windows only | Segoe UI, the system's message font |
| `FontSize` | points, above 0 and up to 200, Windows only | the system's, 9 |
| `Corners` | `"Default"`, `"Round"`, `"Small"`, `"Square"` | `"Default"`, Windows 11 only |

On Windows, high contrast keeps its own colors whatever `ColorScheme` is.

```luau
local theme = require("@Luwow/theme")
local gui = require("@Luwow/gui")

theme.configure({ ColorScheme = "System" })

local window = gui.createComponent({ 
    Type = "Window", 
    Title = "Hello", 
    Children = {
	Go = { 
            Type = "Button", 
            Caption = "Go" 
        }
    }
})

theme.apply(window, { Corners = "Round" })
window:show()
```
