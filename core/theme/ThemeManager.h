#pragma once

#include "theme/AppTheme.h"
#include "theme/ThemePalette.h"

#include <QString>

class SettingsStore; // forward-declared, core/stores/SettingsStore.h

// Thin persistence layer on top of AppTheme's data (Task 26): resolves the
// user's saved theme selection at construction time and exposes it as a
// live name/palette pair, writing back through SettingsStore only when
// setTheme() is explicitly called. Mirrors ThemeManager.swift.
class ThemeManager
{
public:
    explicit ThemeManager(SettingsStore& settingsStore);

    QString themeName() const;
    ThemePalette palette() const;

    // No-op if `name` is not one of AppTheme::themeNames() -- mirrors
    // ThemeManager.swift's `guard AppTheme.palettes[name] != nil else
    // return`. On success, persists via settingsStore.setThemeId(name).
    void setTheme(const QString& name);

private:
    SettingsStore& m_settingsStore;
    QString m_themeName;
    ThemePalette m_palette;
};
