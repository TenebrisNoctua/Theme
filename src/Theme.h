#pragma once

#include "Options.h"

// What each platform's Theme file implements, run on the gui library's main thread
namespace Luwow::Theme {

// Readies the platform's theming as the library loads, at startup before any window is made
void startPlatform();

// Themes the window, the platform's object of a gui Window, and the controls it has and gets later. Its own
// options take the fields options has, over the defaults. Each platform uses the options it has a look for.
void applyTheme(void* window, const Options& options);

// The defaults take the fields options has, and the windows themed are themed again with them
void configureTheme(const Options& options);

} // namespace Luwow::Theme
