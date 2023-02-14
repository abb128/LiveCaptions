/* livecaptions-window.h
 * The main application window, which contains the captions, a microphone
 * button, settings button, and close button.
 *
 * Copyright 2022 abb128
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

struct _LiveCaptionsWindow {
    GtkApplicationWindow  parent_instance;

    GSettings *settings;

    /* Template widgets */
    GtkWidget        *main;
    GtkBox           *side_box;
    GtkBox           *side_box_tiny;
    GtkToggleButton  *mic_button;
    GtkLabel         *label;

    GtkCssProvider *css_provider;

    bool slow_warning_shown;
    GtkWidget *too_slow_warning;
    time_t slow_time;
    
    GtkWidget *slow_warning;
    GtkWidget *slowest_warning;

    volatile PangoLayout *font_layout;
    volatile size_t font_layout_counter;
    volatile int max_text_width;
};

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_WINDOW (livecaptions_window_get_type())

G_DECLARE_FINAL_TYPE (LiveCaptionsWindow, livecaptions_window, LIVECAPTIONS, WINDOW, GtkApplicationWindow);

void livecaptions_window_warn_slow(LiveCaptionsWindow *self);

G_END_DECLS
