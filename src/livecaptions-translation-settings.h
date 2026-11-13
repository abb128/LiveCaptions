/* livecaptions-translation-settings.h
 *
 * Controller for translation preferences and offline language packages.
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

typedef struct _LiveCaptionsTranslationSettings LiveCaptionsTranslationSettings;

LiveCaptionsTranslationSettings *livecaptions_translation_settings_new(
    GObject *owner,
    GSettings *settings,
    AdwSwitchRow *dual_subtitles_switch,
    AdwSwitchRow *translate_on_hover_switch,
    AdwComboRow *target_language_row,
    GtkButton *language_action_button,
    GtkButton *language_refresh_button,
    GtkLabel *status_label,
    GtkAdjustment *settle_adjustment,
    GtkAdjustment *minimum_words_adjustment,
    GtkScale *text_opacity_scale,
    GtkAdjustment *text_opacity_adjustment);

void livecaptions_translation_settings_refresh(
    LiveCaptionsTranslationSettings *self);

void livecaptions_translation_settings_free(
    LiveCaptionsTranslationSettings *self);

G_END_DECLS
