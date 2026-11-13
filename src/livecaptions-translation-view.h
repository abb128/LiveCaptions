/* livecaptions-translation-view.h
 *
 * Pointer tracking and translated-word popover for the captions window.
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_TRANSLATION_VIEW (livecaptions_translation_view_get_type())
G_DECLARE_FINAL_TYPE(LiveCaptionsTranslationView,
                     livecaptions_translation_view,
                     LIVECAPTIONS,
                     TRANSLATION_VIEW,
                     GObject)

LiveCaptionsTranslationView *livecaptions_translation_view_new(
    GtkLabel *caption_label,
    GSettings *settings);

void livecaptions_translation_view_set_translation(
    LiveCaptionsTranslationView *self,
    const gchar *text);

void livecaptions_translation_view_set_opacity(
    LiveCaptionsTranslationView *self,
    gdouble opacity);

void livecaptions_translation_view_clear(LiveCaptionsTranslationView *self);

G_END_DECLS
