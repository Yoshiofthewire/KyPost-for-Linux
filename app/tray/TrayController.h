#pragma once

#include <QObject>

#include <memory>

class GeneralController;
class KStatusNotifierItem;
class QWindow;

// Owns the desktop-only system tray icon (KStatusNotifierItem), created and
// torn down live as GeneralController::trayIconEnabled toggles. Only ever
// constructed in main.cpp when the process resolved to Desktop mode -- see
// GeneralController::isDesktopMode.
class TrayController : public QObject
{
    Q_OBJECT

public:
    TrayController(QWindow* window, GeneralController& general, QObject* parent = nullptr);
    ~TrayController() override;

private:
    void setEnabled(bool enabled);

    QWindow* m_window;
    GeneralController& m_general;
    std::unique_ptr<KStatusNotifierItem> m_item; // null when disabled
};
