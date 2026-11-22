#include "Theme.h"

#include <gtk/gtk.h>

#include <string>

// GTK 3 draws the controls in the desktop's theme, light or dark for the whole application rather than each
// window, so the ColorScheme last given is every window's. Only ColorScheme changes anything.
namespace Luwow::Theme {

namespace {

// Only used on the main thread
ColorScheme scheme = ColorScheme::System;

// GNOME's interface settings, whose color-scheme newer desktops set for dark mode. nullptr where there are none.
GSettings* desktop = nullptr;

bool desktopPrefersDark() {
    if (!desktop) return false;
    gchar* value = g_settings_get_string(desktop, "color-scheme");
    bool dark = value && std::string(value) == "prefer-dark";
    g_free(value);
    return dark;
}

// Gives GTK's settings the scheme.
void update() {
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings) return;

    gtk_settings_reset_property(settings, "gtk-theme-name");
    if (scheme == ColorScheme::Light) {
        gchar* name = nullptr;
        g_object_get(settings, "gtk-theme-name", &name, nullptr);
        std::string theme = name ? name : "";
        g_free(name);
        const std::string suffix = "-dark";
        if (theme.size() > suffix.size() && theme.compare(theme.size() - suffix.size(), suffix.size(), suffix) == 0) {
            g_object_set(settings, "gtk-theme-name", theme.substr(0, theme.size() - suffix.size()).c_str(), nullptr);
        }
    }

    bool dark = scheme == ColorScheme::Dark || (scheme == ColorScheme::System && desktopPrefersDark());
    if (dark || scheme == ColorScheme::Light) g_object_set(settings, "gtk-application-prefer-dark-theme", dark, nullptr);
    else gtk_settings_reset_property(settings, "gtk-application-prefer-dark-theme");
}

void desktopChanged(GSettings* settings, const gchar* key, gpointer data) {
    (void)settings;
    (void)key;
    (void)data;
    if (scheme == ColorScheme::System) update();
}

// The scheme options has, the latest one given being every window's
void take(const Options& options) {
    if (!options.colorScheme) return;
    scheme = *options.colorScheme;
    update();
}

} // namespace

void startPlatform() {
    GSettingsSchemaSource* source = g_settings_schema_source_get_default();
    GSettingsSchema* schema = source ? g_settings_schema_source_lookup(source, "org.gnome.desktop.interface", TRUE) : nullptr;
    if (!schema) return;
    if (g_settings_schema_has_key(schema, "color-scheme")) {
        desktop = g_settings_new("org.gnome.desktop.interface");
        g_signal_connect(desktop, "changed::color-scheme", G_CALLBACK(desktopChanged), nullptr);
    }
    g_settings_schema_unref(schema);
}

void applyTheme(void* window, const Options& options) {
    (void)window;
    take(options);
}

void configureTheme(const Options& options) {
    take(options);
}

} // namespace Luwow::Theme
