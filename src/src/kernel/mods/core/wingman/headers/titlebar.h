#pragma once

#include "./types.h"
#include "./surface.h"

// See docs/DOCS.md ("mods/core/wingman/headers/titlebar.h / mods/core/wingman/window.h -- TitleBar owned by Window" section).
// Pure config/geometry -- no drawing code lives on this class. The band,
// buttons, icon, and title text are drawn by the free function
// draw_title_bar() below, called from WindowManager::composite() rather
// than from each app's own redraw(), so the title band stays correct
// regardless of whether an app remembers to draw it itself.
class TitleBar {
    public:
        TitleBar();
        void configure(
            int height,
            int thickness,
            bool hasCloseButton,
            int contentY,
            bool hasIcon = false,
            int iconId = 0,
            int iconScale = 1,
            int textScale = 2
        );
        int height() const;
        bool hasCloseButton() const;
        bool closeButtonContains(int x, int y) const;
        // Width of the whole close/minimize/maximize zone -- keeps it out of the window's draggable area.
        int closeButtonZoneWidth() const;
    private:
        friend void draw_title_bar(Surface* surface, int windowWidth, const TitleBar& titleBar, const char* title);
    private:
        int _height = 0;
        int _thickness = 0;
        int _contentY = 0;
        int _iconId = 0;
        int _iconScale = 1;
        int _textScale = 2;
        bool _hasCloseButton = false;
        bool _hasIcon = false;
};

// Renders the band, buttons, icon, and title text; a friend of TitleBar so it can read the private fields configure() set.
void draw_title_bar(Surface* surface, int windowWidth, const TitleBar& titleBar, const char* title);
