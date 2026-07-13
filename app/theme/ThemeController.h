#pragma once

#include "theme/SemanticColors.h"
#include "theme/ThemeManager.h"

#include <QColor>
#include <QObject>
#include <QStringList>

class SettingsStore;

// QML-facing wrapper around core::ThemeManager (Task 27): exposes the
// current palette as live QColor properties bound via themeChanged(), plus
// the theme-invariant semantic colors, shape radii, and bundled font family
// names as CONSTANT properties. Registered as a QML singleton in main.cpp
// (qmlRegisterSingletonInstance, so there is exactly one instance sharing
// this app's SettingsStore-backed ThemeManager -- QML must never construct
// its own).
class ThemeController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeChanged)
    Q_PROPERTY(QStringList themeNames READ themeNames CONSTANT)
    Q_PROPERTY(QColor bg READ bg NOTIFY themeChanged)
    Q_PROPERTY(QColor panel READ panel NOTIFY themeChanged)
    Q_PROPERTY(QColor ink READ ink NOTIFY themeChanged)
    Q_PROPERTY(QColor inkStrong READ inkStrong NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentSoft READ accentSoft NOTIFY themeChanged)
    Q_PROPERTY(QColor line READ line NOTIFY themeChanged)
    Q_PROPERTY(QColor readableOnAccent READ readableOnAccent NOTIFY themeChanged)
    Q_PROPERTY(bool isLight READ isLight NOTIFY themeChanged)
    // Semantic colors + shape radii: theme-invariant, CONSTANT.
    Q_PROPERTY(QColor dangerColor READ dangerColor CONSTANT)
    Q_PROPERTY(QColor dangerBorderColor READ dangerBorderColor CONSTANT)
    Q_PROPERTY(QColor dangerFillColor READ dangerFillColor CONSTANT)
    Q_PROPERTY(QColor warningColor READ warningColor CONSTANT)
    Q_PROPERTY(QColor successBorderColor READ successBorderColor CONSTANT)
    Q_PROPERTY(QColor successTextColor READ successTextColor CONSTANT)
    Q_PROPERTY(int shapeField READ shapeField CONSTANT)
    Q_PROPERTY(int shapeButton READ shapeButton CONSTANT)
    Q_PROPERTY(int shapePanel READ shapePanel CONSTANT)
    Q_PROPERTY(int shapeSheet READ shapeSheet CONSTANT)
    Q_PROPERTY(int shapeEmptyState READ shapeEmptyState CONSTANT)
    Q_PROPERTY(QString fontUi READ fontUi CONSTANT)
    Q_PROPERTY(QString fontMono READ fontMono CONSTANT)

public:
    explicit ThemeController(SettingsStore& settingsStore, QObject* parent = nullptr);

    QString themeName() const;
    QStringList themeNames() const;

    QColor bg() const;
    QColor panel() const;
    QColor ink() const;
    QColor inkStrong() const;
    QColor accent() const;
    QColor accentSoft() const;
    QColor line() const;
    QColor readableOnAccent() const;
    bool isLight() const;

    QColor dangerColor() const;
    QColor dangerBorderColor() const;
    QColor dangerFillColor() const;
    QColor warningColor() const;
    QColor successBorderColor() const;
    QColor successTextColor() const;

    int shapeField() const;
    int shapeButton() const;
    int shapePanel() const;
    int shapeSheet() const;
    int shapeEmptyState() const;

    QString fontUi() const;   // "Space Grotesk" -- see Part B / app/resources/fonts.qrc
    QString fontMono() const; // "IBM Plex Mono" -- see Part B / app/resources/fonts.qrc

    Q_INVOKABLE void setTheme(const QString& name);

signals:
    void themeChanged();

private:
    static QColor fromHex(quint32 rgbHex);
    static QColor fromSemantic(const SemanticColor& color);

    ThemeManager m_manager; // core::ThemeManager, owns the SettingsStore&
};
