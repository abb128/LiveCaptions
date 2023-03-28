/* livecaptions-settings.h
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
#include "livecaptions-application.h"

struct _LiveCaptionsSettings {
    AdwPreferencesWindow  parent_instance;

    LiveCaptionsApplication *application;

    GSettings *settings;

    GtkFontButton *font_button;
    GtkSwitch *text_upper_switch;
    GtkSwitch *fade_text_switch;

    AdwActionRow *font_button_ar;
    AdwActionRow *text_upper_switch_ar;
    AdwActionRow *fade_text_switch_ar;

    GtkSwitch *filter_profanity_switch;
    AdwActionRow *filter_profanity_switch_ar;

    GtkSwitch *filter_slurs_switch;
    AdwActionRow *filter_slurs_switch_ar;
    
    GtkSwitch *save_history_switch;
    AdwActionRow *save_history_switch_ar;

    GtkSwitch *keep_above_switch;
    AdwActionRow *keep_above_switch_ar;

    GtkScale *line_width_scale;
    GtkAdjustment *line_width_adjustment;

    GtkScale *window_transparency_scale;
    GtkAdjustment *window_transparency_adjustment;

    GtkLabel *benchmark_label;
    GtkLabel *keep_above_instructions;

    AdwPreferencesGroup *models_list;
    GtkCheckButton *radio_button_1;
};

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_SETTINGS (livecaptions_settings_get_type())

G_DECLARE_FINAL_TYPE(LiveCaptionsSettings, livecaptions_settings, LIVECAPTIONS, SETTINGS, AdwPreferencesWindow)

G_END_DECLS
