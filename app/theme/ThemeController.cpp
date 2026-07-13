#include "theme/ThemeController.h"

#include "theme/Shape.h"

ThemeController::ThemeController(SettingsStore& settingsStore, QObject* parent)
    : QObject(parent)
    , m_manager(settingsStore)
{
}

QColor ThemeController::fromHex(quint32 rgbHex)
{
    return QColor(static_cast<int>((rgbHex >> 16) & 0xFF),
                  static_cast<int>((rgbHex >> 8) & 0xFF),
                  static_cast<int>(rgbHex & 0xFF));
}

QColor ThemeController::fromSemantic(const SemanticColor& color)
{
    QColor qcolor = fromHex(color.rgb);
    qcolor.setAlphaF(static_cast<float>(color.alpha));
    return qcolor;
}

QString ThemeController::themeName() const
{
    return m_manager.themeName();
}

QStringList ThemeController::themeNames() const
{
    return AppTheme::themeNames();
}

QColor ThemeController::bg() const
{
    return fromHex(m_manager.palette().bg);
}

QColor ThemeController::panel() const
{
    return fromHex(m_manager.palette().panel);
}

QColor ThemeController::ink() const
{
    return fromHex(m_manager.palette().ink);
}

QColor ThemeController::inkStrong() const
{
    return fromHex(m_manager.palette().inkStrong);
}

QColor ThemeController::accent() const
{
    return fromHex(m_manager.palette().accent);
}

QColor ThemeController::accentSoft() const
{
    return fromHex(m_manager.palette().accentSoft);
}

QColor ThemeController::line() const
{
    return fromHex(m_manager.palette().line);
}

QColor ThemeController::readableOnAccent() const
{
    return fromHex(m_manager.palette().readableOnAccent());
}

bool ThemeController::isLight() const
{
    return m_manager.palette().isLight();
}

QColor ThemeController::dangerColor() const
{
    return fromSemantic(SemanticColors::danger);
}

QColor ThemeController::dangerBorderColor() const
{
    return fromSemantic(SemanticColors::dangerBorder);
}

QColor ThemeController::dangerFillColor() const
{
    return fromSemantic(SemanticColors::dangerFill);
}

QColor ThemeController::warningColor() const
{
    return fromSemantic(SemanticColors::warning);
}

QColor ThemeController::successBorderColor() const
{
    return fromSemantic(SemanticColors::successBorder);
}

QColor ThemeController::successTextColor() const
{
    return fromSemantic(SemanticColors::successText);
}

int ThemeController::shapeField() const
{
    return Shape::field;
}

int ThemeController::shapeButton() const
{
    return Shape::button;
}

int ThemeController::shapePanel() const
{
    return Shape::panel;
}

int ThemeController::shapeSheet() const
{
    return Shape::sheet;
}

int ThemeController::shapeEmptyState() const
{
    return Shape::emptyState;
}

QString ThemeController::fontUi() const
{
    return QStringLiteral("Space Grotesk");
}

QString ThemeController::fontMono() const
{
    return QStringLiteral("IBM Plex Mono");
}

void ThemeController::setTheme(const QString& name)
{
    const QString before = m_manager.themeName();
    m_manager.setTheme(name);
    if (m_manager.themeName() != before)
        emit themeChanged();
}
