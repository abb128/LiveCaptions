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
#include "livecaptions-translation-settings.h"

struct _LiveCaptionsSettings {
    AdwPreferencesWindow  parent_instance;

    LiveCaptionsApplication *application;

    GSettings *settings;

    GtkFontButton *font_button;
    GtkSwitch *text_upper_switch;
    GtkSwitch *fade_text_switch;

    AdwActionRow *font_button_ar;

    AdwSwitchRow *filter_profanity_switch;
    AdwSwitchRow *filter_slurs_switch;
    AdwSwitchRow *save_history_switch;
    AdwSwitchRow *keep_above_switch;
    AdwActionRow *text_stream_switch;
    AdwSwitchRow *show_dual_subtitles_switch;
    AdwSwitchRow *translate_hover_switch;
    AdwComboRow *target_language_row;
    GtkButton *language_action_button;
    GtkButton *language_refresh_button;
    GtkLabel *translation_status_label;
    GtkScale *translation_delay_scale;
    GtkAdjustment *translation_delay_adjustment;
    GtkScale *translation_min_words_scale;
    GtkAdjustment *translation_min_words_adjustment;
    GtkScale *translation_text_opacity_scale;
    GtkAdjustment *translation_text_opacity_adjustment;
    LiveCaptionsTranslationSettings *translation_settings;

    GtkScale *line_width_scale;
    GtkAdjustment *line_width_adjustment;

    GtkScale *window_transparency_scale;
    GtkAdjustment *window_transparency_adjustment;

    GtkLabel *benchmark_label;
    GtkLabel *keep_above_instructions;

    AdwPreferencesGroup *models_list;
    GtkCheckButton *radio_button_1;

    GtkFileFilter *file_filter;
};

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_SETTINGS (livecaptions_settings_get_type())

G_DECLARE_FINAL_TYPE(LiveCaptionsSettings, livecaptions_settings, LIVECAPTIONS, SETTINGS, AdwPreferencesWindow)

G_END_DECLS
