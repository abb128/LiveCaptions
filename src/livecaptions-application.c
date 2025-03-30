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
#include "livecaptions-history-window.h"
#include "livecaptions-welcome.h"
#include "window-helper.h"
#include "asrproc.h"
#include "common.h"
#include "history.h"

G_DEFINE_TYPE (LiveCaptionsApplication, livecaptions_application, ADW_TYPE_APPLICATION)

static void deinit_audio(LiveCaptionsApplication *self){
    if(self->audio != NULL) {
        free_audio_thread(self->audio);
        self->audio = NULL;
    }
}

static void init_audio(LiveCaptionsApplication *self) {
    deinit_audio(self);

    gboolean use_microphone = g_settings_get_boolean(self->settings, "microphone");
    self->audio = create_audio_thread(use_microphone, self->asr);

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
    asr_thread_pause(self->asr, true);

    save_current_history(default_history_file);

    audio_thread audio = self->audio;

    G_OBJECT_CLASS(livecaptions_application_parent_class)->finalize(object);

    if(audio != NULL) free_audio_thread(audio);
}


static void livecaptions_application_show_welcome(LiveCaptionsApplication *self){
    asr_thread_pause(self->asr, true);
    GtkWindow *window = GTK_WINDOW(self->window);

    gtk_widget_set_visible(GTK_WIDGET(window), false);

    LiveCaptionsWelcome *welcome = g_object_new(LIVECAPTIONS_TYPE_WELCOME, "application", GTK_APPLICATION(self), NULL);
    welcome->application = self;

    gdouble benchmark_result = g_settings_get_double(self->settings, "benchmark");
    livecaptions_set_cancel_enabled(welcome, (benchmark_result >= MINIMUM_BENCHMARK_RESULT));

    gtk_window_present (GTK_WINDOW (welcome));

    self->welcome = GTK_WINDOW(welcome);
}

static void livecaptions_application_activate(GApplication *app) {
    history_init();
    load_history_from(default_history_file);

    GtkWindow *window;

    g_assert(LIVECAPTIONS_IS_APPLICATION(app));
    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(app);

    window = gtk_application_get_active_window(GTK_APPLICATION(app));
    if(window == NULL) {
        window = g_object_new(LIVECAPTIONS_TYPE_WINDOW, "application", GTK_APPLICATION(self), NULL);

        LiveCaptionsWindow *lc_window = LIVECAPTIONS_WINDOW(window);
        asr_thread_set_main_window(self->asr, lc_window);

        asr_thread_set_text_stream_active(self->asr, g_settings_get_boolean(self->settings, "text-stream-active"));
        
        gtk_label_set_text(lc_window->label, " \n ");

        self->window = lc_window;
    }

    gtk_window_present(window);

    gdouble benchmark_result = g_settings_get_double(self->settings, "benchmark");

    if(benchmark_result < MINIMUM_BENCHMARK_RESULT) {
        livecaptions_application_show_welcome(self);
    }

    init_audio(self);
}

static gboolean on_handle_allow_keep_above(DBLCapNetSapplesLiveCaptionsExternal *dbus_external,
                                           GDBusMethodInvocation *invocation,
                                           gpointer user_data)
{
    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(user_data);

    override_keep_above_system(true);

    dblcap_net_sapples_live_captions_external_complete_allow_keep_above(dbus_external, invocation);
    return TRUE;
}

static gboolean
livecaptions_application_dbus_register(GApplication     *app,
                                       GDBusConnection  *connection,
                                       const gchar      *object_path,
                                       GError          **error)
{
    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(app);
 
    self->dbus_external = dblcap_net_sapples_live_captions_external_skeleton_new();

    gboolean success = g_dbus_interface_skeleton_export(
        G_DBUS_INTERFACE_SKELETON(self->dbus_external),
        connection,
        "/net/sapples/LiveCaptions/External",
        error
    );

    if(!success){
        printf("Error registering D-Bus interface\n");
        // Try not to panic here
        return false;
    }


    dblcap_net_sapples_live_captions_external_set_keep_above(
        self->dbus_external,
        g_settings_get_boolean(self->settings, "keep-on-top")
    );

    dblcap_net_sapples_live_captions_external_set_text_stream_active(
        self->dbus_external,
        g_settings_get_boolean(self->settings, "text-stream-active")
    );

    g_signal_connect(self->dbus_external, "handle-allow-keep-above", G_CALLBACK(on_handle_allow_keep_above), self);

    return success;
}


static void
livecaptions_application_dbus_unregister(GApplication    *app,
                                         GDBusConnection *connection,
                                         const gchar     *object_path)
{
    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(app);

    g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(self->dbus_external));

    if(self->dbus_external){
        g_object_unref(self->dbus_external);
        self->dbus_external = NULL;
    }
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

    app_class->dbus_register = livecaptions_application_dbus_register;
    app_class->dbus_unregister = livecaptions_application_dbus_unregister;
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
    LiveCaptionsSettings *preferences = g_object_new(LIVECAPTIONS_TYPE_SETTINGS, "application", GTK_APPLICATION(self), NULL);
    preferences->application = self;

    gtk_window_present (GTK_WINDOW (preferences));

}

static void history_window_destroy_cb(GtkWidget *widget, gpointer data)
{
    LiveCaptionsApplication *self = (LiveCaptionsApplication *)data;
    self->history_window = NULL;
}

static void
livecaptions_application_show_history(G_GNUC_UNUSED GSimpleAction *action,
                                      G_GNUC_UNUSED GVariant     *parameter,
                                      gpointer       user_data)
{
    LiveCaptionsApplication *self = LIVECAPTIONS_APPLICATION(user_data);
    if (self->welcome != NULL) return;

    if (self->history_window != NULL) {
        // If history window already exists, just present it
        gtk_window_present(GTK_WINDOW(self->history_window));
        return;
    }

    // Get the active window
    GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(self));
    if (!GTK_IS_WINDOW(window)) {
        g_warning("No active window found or invalid active window.");
        return;
    }

    // Create a new history window with the active window as its transient parent
    self->history_window = g_object_new(LIVECAPTIONS_TYPE_HISTORY_WINDOW, "transient-for", window, NULL);
    if (!LIVECAPTIONS_IS_HISTORY_WINDOW(self->history_window)) {
        g_warning("Failed to create LiveCaptionsHistoryWindow.");
        self->history_window = NULL;
        return;
    }

    // Connect to the destroy signal to reset the reference when the window is closed
    g_signal_connect(self->history_window, "destroy", G_CALLBACK(history_window_destroy_cb), self);

    gtk_window_present(GTK_WINDOW(self->history_window));
}

static void on_settings_change(G_GNUC_UNUSED GSettings *settings,
                               char      *key,
                               gpointer   user_data){

    LiveCaptionsApplication *self = user_data;

    if(g_str_equal(key, "microphone")) {
        init_audio(self);
        g_simple_action_set_state(self->mic_action, g_variant_new_boolean(g_settings_get_boolean(self->settings, "microphone")));
    }else if(g_str_equal(key, "filter-slurs")) {
        if(g_settings_get_boolean(self->settings, "filter-profanity") && !g_settings_get_boolean(self->settings, "filter-slurs")){
            // Filter slurs was turned off but profanity is still on, this is invalid state, turn off filter profanity
            g_settings_set_boolean(self->settings, "filter-profanity", false);
        }
    }else if(g_str_equal(key, "filter-profanity")){
        if(g_settings_get_boolean(self->settings, "filter-profanity") && !g_settings_get_boolean(self->settings, "filter-slurs")){
            // Filter profanity was turned on but slurs is still off, this is invalid state, turn on slur filter
            g_settings_set_boolean(self->settings, "filter-slurs", true);
        }
    }else if(g_str_equal(key, "keep-on-top")){
        if(self->dbus_external) {
            dblcap_net_sapples_live_captions_external_set_keep_above(
                self->dbus_external,
                g_settings_get_boolean(self->settings, "keep-on-top")
            );
        }
    }else if(g_str_equal(key, "text-stream-active")){
        if(self->dbus_external) {
            gboolean active = g_settings_get_boolean(self->settings, "text-stream-active");
            dblcap_net_sapples_live_captions_external_set_text_stream_active(
                self->dbus_external,
                active
            );

            asr_thread_set_text_stream_active(self->asr, active);
        }
    }
}

static void
livecaptions_application_toggle_microphone(GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data)
{
    gboolean use_microphone;
    LiveCaptionsApplication *self;

    g_assert(LIVECAPTIONS_IS_APPLICATION(user_data));

    self = LIVECAPTIONS_APPLICATION(user_data);
    use_microphone = g_variant_get_boolean(state);

    g_settings_set_boolean(self->settings, "microphone", use_microphone);

    g_simple_action_set_state(action, state);
}

static void livecaptions_application_init(LiveCaptionsApplication *self) {
    self->settings = g_settings_new("net.sapples.LiveCaptions");

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

    g_autoptr(GSimpleAction) history_action = g_simple_action_new("history", NULL);
    g_signal_connect(history_action, "activate", G_CALLBACK(livecaptions_application_show_history), self); // Corrected line
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(history_action));

    gboolean use_microphone = g_settings_get_boolean(self->settings, "microphone");
    g_autoptr(GSimpleAction) mic_action = g_simple_action_new_stateful("microphone", NULL, g_variant_new_boolean(use_microphone));
    g_signal_connect(mic_action, "change-state", G_CALLBACK(livecaptions_application_toggle_microphone), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(mic_action));

    self->mic_action = mic_action;

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


    g_signal_connect(self->settings, "changed", G_CALLBACK(on_settings_change), self);
}


void livecaptions_application_finish_setup(LiveCaptionsApplication *self, gdouble result) {
    gtk_widget_set_visible(GTK_WIDGET(self->window), true);

    gtk_window_close(self->welcome);
    gtk_window_destroy(self->welcome);
    self->welcome = NULL;

    if(result > 0.0)
        g_settings_set_double(self->settings, "benchmark", result);

    asr_thread_pause(self->asr, false);
}

void livecaptions_application_stream_text(LiveCaptionsApplication *self, const char* text) {
    // printf("\n\n----\nSTREAM TEXT:\n%s", text);
    if(self->dbus_external) {
        dblcap_net_sapples_live_captions_external_emit_text_stream(self->dbus_external, text);
    }
}