#pragma once

#ifdef _WIN32

#include "platform/platform.hpp"

#include <gdiplus.h>

#include <filesystem>
#include <memory>

using NativeIcon = HICON;

class IconBackend
{
public:
    IconBackend()
    {
        Gdiplus::GdiplusStartupInput startupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);
    }

    ~IconBackend()
    {
        if (gdiplusToken)
        {
            Gdiplus::GdiplusShutdown(gdiplusToken);
        }
    }

    IconBackend(const IconBackend &) = delete;
    IconBackend &operator=(const IconBackend &) = delete;

    bool Load(const std::filesystem::path &filename, NativeIcon &out) const
    {
        auto bitmap = std::make_unique<Gdiplus::Bitmap>(filename.c_str());

        if (bitmap->GetLastStatus() != Gdiplus::Ok)
        {
            return false;
        }

        HICON icon = nullptr;
        if (bitmap->GetHICON(&icon) != Gdiplus::Ok || icon == nullptr)
        {
            return false;
        }

        out = icon;
        return true;
    }

    static void Destroy(NativeIcon &icon)
    {
        if (icon)
        {
            DestroyIcon(icon);
            icon = nullptr;
        }
    }

private:
    ULONG_PTR gdiplusToken = 0;
};

#else

#include <QIcon>
#include <QPixmap>
#include <QString>

#include <filesystem>

using NativeIcon = QIcon;

class IconBackend
{
public:
    IconBackend() = default;

    IconBackend(const IconBackend &) = delete;
    IconBackend &operator=(const IconBackend &) = delete;

    bool Load(const std::filesystem::path &filename, NativeIcon &out) const
    {
        QPixmap pixmap(QString::fromStdString(filename.string()));

        if (pixmap.isNull())
        {
            return false;
        }

        out = QIcon(pixmap);
        return true;
    }

    // The loader outlives QApplication; pixmap-backed icons must be dropped
    // while Qt is still alive (see Application::shutdown).
    static void Destroy(NativeIcon &icon) { icon = QIcon(); }
};

#endif
