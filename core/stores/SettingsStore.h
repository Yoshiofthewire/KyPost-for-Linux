#pragma once

#include <QSettings>
#include <QString>
#include <optional>

// Thin typed wrapper around QSettings for app-wide preferences. The
// Connection and Keywords sections from the plan have no fields yet (see
// task notes) and are intentionally not represented here. Construct with an
// explicit file path so callers (real app or tests) control where settings
// live; core/ never decides the real on-disk location itself.
class SettingsStore
{
public:
    explicit SettingsStore(const QString& filePath);

    // Appearance
    QString themeId() const;
    void setThemeId(const QString& themeId);

    std::optional<bool> manualMobileOverride() const;
    void setManualMobileOverride(std::optional<bool> override);

    // Notifications
    QString pushServerBaseUrl() const;
    void setPushServerBaseUrl(const QString& baseUrl);

    // Push delivery (set by DeviceRegistrationService on successful (re-)registration)
    QString deliveryMode() const;      // "push" or "pull", empty if never registered
    void setDeliveryMode(const QString& mode);

    QString pullEndpoint() const;
    void setPullEndpoint(const QString& endpoint);

    QString transport() const;         // server-normalized value from the last successful registration
    void setTransport(const QString& transport);

private:
    QSettings m_settings;
};
