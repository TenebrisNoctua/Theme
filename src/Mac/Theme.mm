#import <Cocoa/Cocoa.h>

#include "Theme.h"

#include <algorithm>
#include <vector>

// macOS draws its controls in the system's look, light or dark by each window's appearance. Only ColorScheme
// changes anything.
namespace Luwow::Theme {

namespace {

// A window themed, gone from the list once the gui library lets it go
struct ThemedWindow {
    __weak NSWindow* window;
    // Its own options, over the defaults
    Options options;
};

// Only used on the main thread
std::vector<ThemedWindow> windows;
Options defaults;

// nil follows the system's appearance, and changes with it
NSAppearance* appearanceOf(ColorScheme scheme) {
    if (@available(macOS 10.14, *)) {
        if (scheme == ColorScheme::Dark) return [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    }
    if (scheme == ColorScheme::Light) return [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    return nil;
}

void themeWindow(ThemedWindow& themed) {
    Options options = defaults;
    options.merge(themed.options);
    [themed.window setAppearance:appearanceOf(options.colorScheme.value_or(ColorScheme::System))];
}

void forgetReleased() {
    windows.erase(std::remove_if(windows.begin(), windows.end(), [](const ThemedWindow& themed) { return themed.window == nil; }), windows.end());
}

} // namespace

void startPlatform() {}

void applyTheme(void* native, const Options& options) {
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)native;
        forgetReleased();
        auto found = std::find_if(windows.begin(), windows.end(), [&](const ThemedWindow& themed) { return themed.window == window; });
        if (found == windows.end()) {
            windows.push_back({ window, {} });
            found = windows.end() - 1;
        }
        found->options.merge(options);
        themeWindow(*found);
    }
}

void configureTheme(const Options& options) {
    @autoreleasepool {
        defaults.merge(options);
        forgetReleased();
        for (ThemedWindow& themed : windows) themeWindow(themed);
    }
}

} // namespace Luwow::Theme
