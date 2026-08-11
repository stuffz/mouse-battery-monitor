#pragma once

#ifdef _WIN32

#include <windows.h>

#include <shellapi.h>

#include <string>

#include "core/logger.hpp"

class TrayIcon
{
public:
    TrayIcon() = default;
    ~TrayIcon() = default;

    TrayIcon(const TrayIcon &) = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;

    void init(HWND hwnd, UINT callbackMessage, UINT iconId, HICON icon, const std::wstring &tooltip)
    {
        nid = {};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = hwnd;
        nid.uID = iconId;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = callbackMessage;
        nid.hIcon = icon;
        wcscpy_s(nid.szTip, tooltip.c_str());

        if (!Shell_NotifyIconW(NIM_ADD, &nid))
        {
            const DWORD err = GetLastError();
            LOG_ERROR("Shell_NotifyIconW failed: " + std::to_string(err));
        }
        else
        {
            LOG_DEBUG("Tray icon added successfully");
        }
    }

    void update(HICON icon, const std::wstring &tooltip)
    {
        nid.hIcon = icon;
        wcscpy_s(nid.szTip, tooltip.c_str());
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }

    void showNotification(const std::wstring &title, const std::wstring &message,
                          DWORD flags = NIIF_WARNING)
    {
        nid.uFlags |= NIF_INFO;
        wcscpy_s(nid.szInfoTitle, title.c_str());
        wcscpy_s(nid.szInfo, message.c_str());
        nid.dwInfoFlags = flags;
        Shell_NotifyIconW(NIM_MODIFY, &nid);
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }

    void remove() { Shell_NotifyIconW(NIM_DELETE, &nid); }

    void reAdd()
    {
        LOG_INFO("Re-adding tray icon");
        Shell_NotifyIconW(NIM_ADD, &nid);
    }

private:
    NOTIFYICONDATAW nid{};
};

#else

#include <QIcon>
#include <QMenu>
#include <QString>
#include <QSystemTrayIcon>

#include <memory>
#include <string>

#include "core/logger.hpp"

class TrayIcon
{
public:
    TrayIcon() = default;
    ~TrayIcon() = default;

    TrayIcon(const TrayIcon &) = delete;
    TrayIcon &operator=(const TrayIcon &) = delete;

    void init(const QIcon &icon, const std::wstring &tooltip)
    {
        trayIcon = std::make_unique<QSystemTrayIcon>();
        trayIcon->setIcon(icon);
        trayIcon->setToolTip(QString::fromStdWString(tooltip));
        trayIcon->show();

        LOG_DEBUG("Tray icon added successfully");
    }

    void update(const QIcon &icon, const std::wstring &tooltip)
    {
        if (!trayIcon)
        {
            return;
        }

        trayIcon->setIcon(icon);
        trayIcon->setToolTip(QString::fromStdWString(tooltip));
    }

    void showNotification(const std::wstring &title, const std::wstring &message)
    {
        if (!trayIcon)
        {
            return;
        }

        trayIcon->showMessage(QString::fromStdWString(title), QString::fromStdWString(message),
                              QSystemTrayIcon::Warning, NOTIFICATION_TIMEOUT_MS);
    }

    void setContextMenu(QMenu *menu)
    {
        if (trayIcon)
        {
            trayIcon->setContextMenu(menu);
        }
    }

    void remove()
    {
        if (trayIcon)
        {
            trayIcon->hide();
            trayIcon.reset();
        }
    }

    QSystemTrayIcon *handle() const { return trayIcon.get(); }

private:
    static constexpr int NOTIFICATION_TIMEOUT_MS = 10000;

    std::unique_ptr<QSystemTrayIcon> trayIcon;
};

#endif
