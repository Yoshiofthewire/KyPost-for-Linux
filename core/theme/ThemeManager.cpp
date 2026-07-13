#include "theme/ThemeManager.h"

#include "stores/SettingsStore.h"

ThemeManager::ThemeManager(SettingsStore& settingsStore)
    : m_settingsStore(settingsStore)
{
    QString name = m_settingsStore.themeId();
    if (name.isEmpty() || !AppTheme::themeNames().contains(name))
        name = AppTheme::defaultThemeName();

    // Read defensively, don't mutate on read (matches PairingStore::load()) --
    // an invalid/missing themeId is not written back here; only setTheme()
    // persists.
    m_themeName = name;
    m_palette = AppTheme::palette(m_themeName);
}

QString ThemeManager::themeName() const
{
    return m_themeName;
}

ThemePalette ThemeManager::palette() const
{
    return m_palette;
}

void ThemeManager::setTheme(const QString& name)
{
    if (!AppTheme::themeNames().contains(name))
        return;

    m_themeName = name;
    m_palette = AppTheme::palette(m_themeName);
    m_settingsStore.setThemeId(m_themeName);
}
