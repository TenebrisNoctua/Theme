#include "Options.h"

#include "lua.h"
#include "lualib.h"

#include <cstring>
#include <utility>

namespace Luwow::Theme {

namespace {

// The choice named by the string at the top of the stack, which a field's value must be one of
template <typename Choice, size_t Count>
Choice readChoice(lua_State* L, const char* field, const char* expected, const std::pair<const char*, Choice> (&choices)[Count]) {
    if (lua_type(L, -1) == LUA_TSTRING) {
        const char* value = lua_tostring(L, -1);
        for (const auto& [name, choice] : choices) {
            if (std::strcmp(value, name) == 0) return choice;
        }
    }
    luaL_error(L, "%s must be %s", field, expected);
    return choices[0].second;
}

const std::pair<const char*, ColorScheme> ColorSchemes[] = {
    { "System", ColorScheme::System },
    { "Light", ColorScheme::Light },
    { "Dark", ColorScheme::Dark },
};

const std::pair<const char*, Corners> CornerChoices[] = {
    { "Default", Corners::Default },
    { "Round", Corners::Round },
    { "Small", Corners::Small },
    { "Square", Corners::Square },
};

} // namespace

void Options::merge(const Options& other) {
    if (other.colorScheme) colorScheme = other.colorScheme;
    if (other.fontName) fontName = other.fontName;
    if (other.fontSize) fontSize = other.fontSize;
    if (other.corners) corners = other.corners;
}

Options readOptions(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    index = lua_absindex(L, index);
    Options options;
    lua_pushnil(L);
    while (lua_next(L, index)) {
        const char* field = lua_type(L, -2) == LUA_TSTRING ? lua_tostring(L, -2) : "";
        if (std::strcmp(field, "ColorScheme") == 0) {
            options.colorScheme = readChoice(L, field, "\"System\", \"Light\" or \"Dark\"", ColorSchemes);
        } else if (std::strcmp(field, "FontName") == 0) {
            if (lua_type(L, -1) != LUA_TSTRING || lua_objlen(L, -1) == 0) luaL_error(L, "FontName must be a font's name");
            options.fontName = lua_tostring(L, -1);
        } else if (std::strcmp(field, "FontSize") == 0) {
            double size = lua_type(L, -1) == LUA_TNUMBER ? lua_tonumber(L, -1) : 0;
            if (!(size > 0 && size <= 200)) luaL_error(L, "FontSize must be a size in points, above 0 and up to 200");
            options.fontSize = size;
        } else if (std::strcmp(field, "Corners") == 0) {
            options.corners = readChoice(L, field, "\"Default\", \"Round\", \"Small\" or \"Square\"", CornerChoices);
        } else {
            lua_pushvalue(L, -2);
            luaL_error(L, "the theme options have no field '%s', expected ColorScheme, FontName, FontSize or Corners", luaL_tolstring(L, -1, nullptr));
        }
        lua_pop(L, 1);
    }
    return options;
}

} // namespace Luwow::Theme
