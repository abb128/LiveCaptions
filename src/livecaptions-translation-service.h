/* livecaptions-translation-service.h
 *
 * The translation worker and its private D-Bus interface.
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_TRANSLATION_SERVICE (livecaptions_translation_service_get_type())
G_DECLARE_FINAL_TYPE(LiveCaptionsTranslationService,
                     livecaptions_translation_service,
                     LIVECAPTIONS,
                     TRANSLATION_SERVICE,
                     GObject)

LiveCaptionsTranslationService *livecaptions_translation_service_new(GSettings *settings);

gboolean livecaptions_translation_service_register_dbus(
    LiveCaptionsTranslationService *self,
    GDBusConnection *connection,
    GError **error);

void livecaptions_translation_service_unregister_dbus(
    LiveCaptionsTranslationService *self,
    GDBusConnection *connection);

void livecaptions_translation_service_update(LiveCaptionsTranslationService *self);
gboolean livecaptions_translation_service_is_enabled(LiveCaptionsTranslationService *self);
void livecaptions_translation_service_request_hover_word(
    LiveCaptionsTranslationService *self,
    const gchar *word);

G_END_DECLS
