/* livecaptions-application.h
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
#include "audiocap.h"
#include "livecaptions-window.h"
#include "livecaptions-history-window.h"
#include "dbus-interface.h"

struct _LiveCaptionsApplication {
    AdwApplication parent_instance;

    GSimpleAction *mic_action;

    GSettings *settings;
    LiveCaptionsWindow *window;
    GtkWindow *welcome;

    LiveCaptionsHistoryWindow *history_window;

    asr_thread asr;
    audio_thread audio;

    DBLCapNetSapplesLiveCaptionsExternal *dbus_external;
};

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_APPLICATION (livecaptions_application_get_type())

G_DECLARE_FINAL_TYPE (LiveCaptionsApplication, livecaptions_application, LIVECAPTIONS, APPLICATION, AdwApplication)


void livecaptions_application_finish_setup(LiveCaptionsApplication *self, gdouble result);

LiveCaptionsApplication *livecaptions_application_new (gchar *application_id,
                                                       GApplicationFlags  flags);

void livecaptions_application_stream_text(LiveCaptionsApplication *self, const char* text);

G_END_DECLS
