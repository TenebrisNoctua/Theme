#pragma once

#include <optional>
#include <string>

struct lua_State;

namespace Luwow::Theme {

enum class ColorScheme { System, Light, Dark };
enum class Corners { Default, Round, Small, Square };

// How windows are themed. A field left out is the defaults', and where they leave it out too, the platform's.
struct Options {
    std::optional<ColorScheme> colorScheme;
    std::optional<std::string> fontName;
    // In points
    std::optional<double> fontSize;
    std::optional<Corners> corners;

    // Takes each field other has
    void merge(const Options& other);
};

// The options table at index, erroring at a field that's wrong
Options readOptions(lua_State* L, int index);

} // namespace Luwow::Theme
