/* livecaptions-window.h
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

    /* Template widgets */
    GtkWidget        *main;
    GtkLabel         *label;

};

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_WINDOW (livecaptions_window_get_type())

G_DECLARE_FINAL_TYPE (LiveCaptionsWindow, livecaptions_window, LIVECAPTIONS, WINDOW, GtkApplicationWindow)

G_END_DECLS
