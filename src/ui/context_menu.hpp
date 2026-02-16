#pragma once

#include <windows.h>
#include <vector>

class ContextMenu
{
public:
    ContextMenu() = default;

    void addItem(UINT id, const wchar_t *label, bool visible = true)
    {
        items.push_back({id, label, visible, false});
    }

    void addSeparator()
    {
        items.push_back({0, nullptr, true, true});
    }

    void clear()
    {
        items.clear();
    }

    void show(HWND hwnd)
    {
        POINT pt;
        GetCursorPos(&pt);

        HMENU menu = CreatePopupMenu();
        for (const auto &item : items)
        {
            if (!item.visible)
                continue;
            if (item.separator)
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            else
                AppendMenuW(menu, MF_STRING, item.id, item.label);
        }

        SetForegroundWindow(hwnd);
        TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
        DestroyMenu(menu);
    }

private:
    struct MenuItem
    {
        UINT id;
        const wchar_t *label;
        bool visible;
        bool separator;
    };

    std::vector<MenuItem> items;
};
