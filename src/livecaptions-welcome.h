/* livecaptions-welcome.h
 * The LiveCaptionsWelcome window is the first-time setup window, which
 * performs a benchmark and warns the user about accuracy.
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
#include <time.h>
#include "livecaptions-application.h"

struct _LiveCaptionsWelcome {
    AdwApplicationWindow  parent_instance;

    LiveCaptionsApplication *application;

    GSettings *settings;

    GtkStack *stack;

    GtkStackPage *initial_page;
    GtkStackPage *benching_page;
    GtkStackPage *benchmark_result_good;
    GtkStackPage *accuracy_page;
    GtkStackPage *benchmark_result_bad;

    GtkButton *cancel_button;
    GtkButton *quit_button;

    GtkProgressBar *benchmark_progress;

    GThread *benchmark_thread;

    volatile gdouble benchmark_progress_v;
    volatile gdouble benchmark_result_v;

    time_t quit_time;
};

G_BEGIN_DECLS

#define LIVECAPTIONS_TYPE_WELCOME (livecaptions_welcome_get_type())

G_DECLARE_FINAL_TYPE (LiveCaptionsWelcome, livecaptions_welcome, LIVECAPTIONS, WELCOME, AdwApplicationWindow)

void livecaptions_set_cancel_enabled(LiveCaptionsWelcome *self, bool enabled);

G_END_DECLS
