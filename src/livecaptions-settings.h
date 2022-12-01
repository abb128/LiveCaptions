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

#include <adwaita.h>

struct _LiveCaptionsSettings {
    AdwPreferencesWindow  parent_instance;

    GSettings *settings;

    GtkFontButton *font_button;
    GtkSwitch *text_upper_switch;
    GtkSwitch *fade_text_switch;
    GtkSwitch *filter_profanity_switch;

    GtkWidget *subpage1;
    GtkWidget *subpage2;

};

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_SETTINGS (livecaptions_settings_get_type())

G_DECLARE_FINAL_TYPE(LiveCaptionsSettings, livecaptions_settings, LIVECAPTIONS, SETTINGS, AdwPreferencesWindow)

G_END_DECLS
