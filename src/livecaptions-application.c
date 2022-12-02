/* livecaptions-application.c
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

#include "livecaptions-application.h"
#include "livecaptions-settings.h"
#include "livecaptions-window.h"
#include "livecaptions-welcome.h"
#include "asrproc.h"
#include "common.h"

G_DEFINE_TYPE (LiveCaptionsApplication, livecaptions_application, ADW_TYPE_APPLICATION)

static void deinit_audio(LiveCaptionsApplication *self){
    if(self->audio != NULL) {
        free_audio_thread(self->audio);
        self->audio = NULL;
    }
}

static void init_audio(LiveCaptionsApplication *self) {
    deinit_audio(self);

    self->audio = create_audio_thread(g_settings_get_boolean(self->settings, "microphone"), self->asr);

    asr_thread_flush(self->asr);
}

LiveCaptionsApplication *livecaptions_application_new (gchar *application_id, GApplicationFlags flags) {
    return g_object_new(LIVECAPTIONS_TYPE_APPLICATION,
                        "application-id", application_id,
                        "flags", flags,
                        NULL);
}

static void livecaptions_application_finalize(GObject *object) {
    LiveCaptionsApplication *self = (LiveCaptionsApplication *)object;

    audio_thread audio = self->audio;

    G_OBJECT_CLASS(livecaptions_application_parent_class)->finalize(object);

    if(audio != NULL) free_audio_thread(audio);
}


static void
livecaptions_application_show_welcome(LiveCaptionsApplication *self){
    asr_thread_pause(self->asr, true);
    GtkApplication *app = GTK_APPLICATION(self);
    GtkWindow *window = gtk_application_get_active_window(app);

    LiveCaptionsWelcome *welcome = g_object_new(LIVECAPTIONS_TYPE_WELCOME, "application", app, NULL);
    welcome->application = self;

    gtk_window_set_transient_for (GTK_WINDOW (welcome), window);
    gtk_window_present (GTK_WINDOW (welcome));

    self->welcome = GTK_WINDOW(welcome);
}

static void livecaptions_application_activate(GApplication *app) {
    GtkWindow *window;

    g_assert(LIVECAPTIONS_IS_APPLICATION(app));
    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(app);

    window = gtk_application_get_active_window(GTK_APPLICATION(app));
    if(window == NULL) {
        window = g_object_new(LIVECAPTIONS_TYPE_WINDOW, "application", app, NULL);

        LiveCaptionsWindow *lc_window = LIVECAPTIONS_WINDOW(window);
        asr_thread_set_label(self->asr, lc_window->label);
        gtk_label_set_text(lc_window->label, " \n ");
    }
    
    gtk_window_present(window);


    gdouble benchmark_result = g_settings_get_double(self->settings, "benchmark");

    if(benchmark_result < MINIMUM_BENCHMARK_RESULT) {
        livecaptions_application_show_welcome(self);
    }

    init_audio(self);
}


static void livecaptions_application_class_init(LiveCaptionsApplicationClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

    object_class->finalize = livecaptions_application_finalize;

    /*
    * We connect to the activate callback to create a window when the application
    * has been launched. Additionally, this callback notifies us when the user
    * tries to launch a "second instance" of the application. When they try
    * to do that, we'll just present any existing window.
    */
    app_class->activate = livecaptions_application_activate;
}


static void
livecaptions_application_show_setup(G_GNUC_UNUSED GSimpleAction *action,
                                    G_GNUC_UNUSED GVariant      *parameter,
                                     gpointer       user_data)
{

    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(user_data);
    livecaptions_application_show_welcome(self);
}

static void
livecaptions_application_show_about(G_GNUC_UNUSED GSimpleAction *action,
                                    G_GNUC_UNUSED GVariant      *parameter,
                                     gpointer       user_data)
{
    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(user_data);
    GtkWindow *window = NULL;
    const gchar *authors[] = {"abb128", NULL};

    g_return_if_fail(LIVECAPTIONS_IS_APPLICATION(self));

    window = gtk_application_get_active_window(GTK_APPLICATION(self));

    gtk_show_about_dialog(window,
                           "program-name", "livecaptions",
                           "authors", authors,
                           "version", "0.1.0",
                             NULL);
}


static void
livecaptions_application_show_preferences(G_GNUC_UNUSED GSimpleAction *action,
                                          G_GNUC_UNUSED GVariant     *parameter,
                                          gpointer       user_data)
{
    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(user_data);
    if(self->welcome != NULL) return;

    GtkApplication *app = GTK_APPLICATION(user_data);
    GtkWindow *window = gtk_application_get_active_window (app);
    LiveCaptionsSettings *preferences = g_object_new(LIVECAPTIONS_TYPE_SETTINGS, "application", app, NULL);

    gtk_window_set_transient_for (GTK_WINDOW (preferences), window);
    gtk_window_present (GTK_WINDOW (preferences));

}


static void on_settings_change(G_GNUC_UNUSED GSettings *settings,
                               char      *key,
                               gpointer   user_data){

    LiveCaptionsApplication *self = user_data;

    if(g_str_equal(key, "microphone")) {
        init_audio(self);
    }
}

static void livecaptions_application_init(LiveCaptionsApplication *self) {
    g_autoptr(GSimpleAction) quit_action = g_simple_action_new("quit", NULL);
    g_signal_connect_swapped(quit_action, "activate", G_CALLBACK(g_application_quit), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(quit_action));

    g_autoptr(GSimpleAction) about_action = g_simple_action_new("about", NULL);
    g_signal_connect(about_action, "activate", G_CALLBACK(livecaptions_application_show_about), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(about_action));

    g_autoptr(GSimpleAction) setup_action = g_simple_action_new("setup", NULL);
    g_signal_connect(setup_action, "activate", G_CALLBACK(livecaptions_application_show_setup), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(setup_action));

    g_autoptr(GSimpleAction) prefs_action = g_simple_action_new("preferences", NULL);
    g_signal_connect(prefs_action, "activate", G_CALLBACK(livecaptions_application_show_preferences), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(prefs_action));

    gtk_application_set_accels_for_action(GTK_APPLICATION(self),
                                           "app.quit",
                                           (const char *[]) {
                                             "<primary>q",
                                             NULL,
                                           });

    gtk_application_set_accels_for_action(GTK_APPLICATION(self),
                                           "app.preferences",
                                           (const char *[]) {
                                             "<primary>p",
                                             NULL,
                                           });

    self->settings = g_settings_new("net.sapples.LiveCaptions");

    g_signal_connect(self->settings, "changed", G_CALLBACK(on_settings_change), self);
    
}


void livecaptions_application_finish_setup(LiveCaptionsApplication *self, gdouble result) {
    gtk_window_close(self->welcome);
    gtk_window_destroy(self->welcome);
    self->welcome = NULL;

    g_settings_set_double(self->settings, "benchmark", result);

    livecaptions_application_show_preferences(NULL, NULL, self);
    asr_thread_pause(self->asr, false);
}
