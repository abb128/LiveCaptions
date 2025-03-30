/* livecaptions-settings.c
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

#include <glib/gi18n.h>

#include "common.h"
#include "window-helper.h"
#include "livecaptions-config.h"
#include "livecaptions-settings.h"
#include "livecaptions-application.h"
#include "history.h"
#include "livecaptions-history-window.h"
#include "asrproc.h"

G_DEFINE_TYPE(LiveCaptionsSettings, livecaptions_settings, ADW_TYPE_PREFERENCES_WINDOW)

static void rerun_benchmark_cb (LiveCaptionsSettings *self) {
    gtk_window_close(GTK_WINDOW(self));

    gtk_widget_activate_action(GTK_WIDGET(self), "app.setup", NULL);
    
    gtk_window_destroy(GTK_WINDOW(self));
}

static void about_cb(LiveCaptionsSettings *self) {
    GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (self));
    GtkWidget *about;

    const char *developers[] = {
        "abb128",
        NULL
    };

    const char *special_thanks[] = {
        "Fangjun Kuang (@csukuangfj) and the k2-fsa/icefall contributors",
        NULL
    };

    about =
        g_object_new(ADW_TYPE_ABOUT_WINDOW,
                    "transient-for", root,
                    "application-icon", "net.sapples.LiveCaptions",
                    "application-name", _("Live Captions"),
                    "developer-name", _("abb128"),
                    "version", LIVECAPTIONS_VERSION,
                    "website", "https://github.com/abb128/LiveCaptions",
                    "issue-url", "https://github.com/abb128/LiveCaptions/issues",
                    "support-url", "https://discord.gg/QWaJHxWjUM",
                    "copyright", "Copyright © 2022 abb128",
                    "license-type", GTK_LICENSE_GPL_3_0,
                    "developers", developers,
                    "translator-credits", _("translator-credits"),
                    NULL);

    adw_about_window_add_legal_section(ADW_ABOUT_WINDOW(about),
                                       _("Model"),
                                       NULL,
                                       GTK_LICENSE_CUSTOM,
                                       "The ASR model was originally trained by Fangjun Kuang (@csukuangfj), and has been finetuned on extra data.");

    adw_about_window_add_legal_section(ADW_ABOUT_WINDOW(about),
                                       "april-asr",
                                       "Copyright © 2022 abb128",
                                       GTK_LICENSE_GPL_3_0_ONLY,
                                       NULL);

    adw_about_window_add_legal_section(ADW_ABOUT_WINDOW(about),
                                       "ONNXRuntime",
                                       "Copyright © 2021 Microsoft Corporation",
                                       GTK_LICENSE_MIT_X11,
                                       NULL);

    adw_about_window_add_legal_section(ADW_ABOUT_WINDOW(about),
                                       "pocketfft",
                                       "Copyright © 2010-2019 Max-Planck-Society",
                                       GTK_LICENSE_BSD_3,
                                       NULL);

    adw_about_window_add_legal_section(ADW_ABOUT_WINDOW(about),
                                       "sonic",
                                       "Copyright © 2010 Bill Cox",
                                       GTK_LICENSE_APACHE_2_0,
                                       NULL);

    adw_about_window_add_acknowledgement_section(ADW_ABOUT_WINDOW(about),
                                                 _("Special thanks to"),
                                                 special_thanks);

    gtk_window_present(GTK_WINDOW(about));
}


static void report_cb(LiveCaptionsSettings *self) {
    gtk_show_uri(
        GTK_WINDOW(self),
        "https://github.com/abb128/LiveCaptions/issues/48",
        GDK_CURRENT_TIME
    );
}

static void download_models_cb(LiveCaptionsSettings *self) {
    gtk_show_uri(
        GTK_WINDOW(self),
        "https://abb128.github.io/april-asr/models.html",
        GDK_CURRENT_TIME
    );
}

static void open_history(LiveCaptionsSettings *self) {
    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(self));

    LiveCaptionsHistoryWindow *window = g_object_new(LIVECAPTIONS_TYPE_HISTORY_WINDOW, "transient-for", root, NULL);
    
    gtk_window_present(GTK_WINDOW(window));

    gtk_window_close(GTK_WINDOW(self));
    gtk_window_destroy(GTK_WINDOW(self));
}

static const char *get_always_on_top_tip_text(){
    const char *desktop = getenv("XDG_CURRENT_DESKTOP");
    if(desktop == NULL) return NULL;

    if(strstr(desktop, "GNOME")){
        return _("Right-click the captions window and enable \"Always on Top\" to keep the captions on top.");
    }else if(strstr(desktop, "KDE")){
        return _("Right-click the captions window (or hit ALT+F3) and enable \"More Actions\" -> \"Keep Above Others\" to keep the captions on top.");
    }else{
        return NULL;
    }
}


static void on_add_model_response(GtkNativeDialog *native,
                                  int        response,
                                  LiveCaptionsSettings *self);

static void add_model_cb(LiveCaptionsSettings *self) {
    GtkFileChooserNative *native;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

    native = gtk_file_chooser_native_new("Load Model",
                                         GTK_WINDOW(self),
                                         action,
                                         _("_Load"),
                                         _("_Cancel"));

    chooser = GTK_FILE_CHOOSER(native);
    gtk_file_chooser_add_filter(chooser, self->file_filter);

    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));

    g_signal_connect(native, "response",
                     G_CALLBACK (on_add_model_response),
                     self);
}

static void on_builtin_toggled(LiveCaptionsSettings *self);

static void livecaptions_settings_class_init(LiveCaptionsSettingsClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-settings.ui");

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, font_button);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, font_button_ar);
    
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, text_upper_switch);
    
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, fade_text_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, filter_profanity_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, filter_slurs_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, save_history_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, keep_above_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, text_stream_switch);

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, line_width_scale);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, line_width_adjustment);

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, window_transparency_scale);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, window_transparency_adjustment);

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, benchmark_label);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, keep_above_instructions);

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, models_list);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, radio_button_1);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, file_filter);
    
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, openai_url_entry);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, openai_key_entry);

    gtk_widget_class_bind_template_callback (widget_class, report_cb);
    gtk_widget_class_bind_template_callback (widget_class, about_cb);
    gtk_widget_class_bind_template_callback (widget_class, rerun_benchmark_cb);
    gtk_widget_class_bind_template_callback (widget_class, open_history);
    gtk_widget_class_bind_template_callback (widget_class, add_model_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_builtin_toggled);
    gtk_widget_class_bind_template_callback (widget_class, download_models_cb);
}

// The settings window needs to be kept on top if the main window is kept on top,
// otherwise the settings will appear under the main window which is not ideal
static gboolean deferred_update_keep_above(void *userdata) {
    LiveCaptionsSettings *self = userdata;

    set_window_keep_above(GTK_WINDOW(self), g_settings_get_boolean(self->settings, "keep-on-top"));

    return G_SOURCE_REMOVE;
}

static void model_load_failsafe(LiveCaptionsSettings *self, bool load_default);
static void on_model_selected(GtkCheckButton* button, LiveCaptionsSettings *self){
    if(!gtk_check_button_get_active(button)) return;

    const char *model = g_quark_to_string((GQuark)g_object_get_data(G_OBJECT(button), "lcap-model-path"));
    if(!asr_thread_update_model(self->application->asr, model)) {
        model_load_failsafe(self, false);
        return;
    }

    g_settings_set_string(self->settings, "active-model", model);
}

static void on_builtin_toggled(LiveCaptionsSettings *self) {
    if(!gtk_check_button_get_active(self->radio_button_1)) return;

    const char *model_path = GET_MODEL_PATH();
    asr_thread_update_model(self->application->asr, model_path);
    g_settings_set_string(self->settings, "active-model", model_path);
}

static void on_model_deleted(GtkButton *button, LiveCaptionsSettings *self) {
    const char *model = g_quark_to_string((GQuark)g_object_get_data(G_OBJECT(button), "lcap-model-path"));
    if(g_str_equal(model, "/app/LiveCaptions/models/aprilv0_en-us.april")) return;

    char *active_model = g_settings_get_string(self->settings, "active-model");
    if(g_str_equal(model, active_model)) {
        gtk_check_button_set_active(self->radio_button_1, true);
        on_builtin_toggled(self);
    }

    gchar **models = g_settings_get_strv(self->settings, "installed-models");
    GStrvBuilder *builder = g_strv_builder_new();

    for(int i=0; models[i] != NULL; i++){
        if(g_str_equal(models[i], model)) continue;
        g_strv_builder_add(builder, models[i]);
    }

    g_strfreev(models);

    GStrv result = g_strv_builder_end(builder);

    g_settings_set_strv(self->settings, "installed-models", (const gchar * const *)result);

    g_strfreev(result);
    g_strv_builder_unref(builder);

    AdwActionRow *row = ADW_ACTION_ROW(gtk_widget_get_parent(gtk_widget_get_parent(gtk_widget_get_parent(GTK_WIDGET(button)))));

    adw_preferences_group_remove(self->models_list, GTK_WIDGET(row));
}

static void insert_model_to_list(LiveCaptionsSettings *self, gchar *model) {
    if(g_str_equal(model, "/app/LiveCaptions/models/aprilv0_en-us.april")) return;

    GtkCheckButton *first = self->radio_button_1;
    AdwActionRow *row = g_object_new(ADW_TYPE_ACTION_ROW, NULL);

    GtkCheckButton *button = g_object_new(GTK_TYPE_CHECK_BUTTON, NULL);
    gtk_widget_set_valign(GTK_WIDGET(button), GTK_ALIGN_CENTER);

    GtkWidget *delete_button = gtk_button_new_from_icon_name("delete-symbolic");
    gtk_widget_add_css_class(delete_button, "flat");
    gtk_widget_set_valign(delete_button, GTK_ALIGN_CENTER);

    gchar *name = g_utf8_strrchr(model, -1, (gunichar)('/')) + 1;

    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), name);
    adw_preferences_group_add(self->models_list, GTK_WIDGET(row));
    adw_action_row_add_prefix(row, GTK_WIDGET(button));
    adw_action_row_add_suffix(row, delete_button);

    adw_action_row_set_activatable_widget(row, GTK_WIDGET(button));

    gtk_check_button_set_group(button, first);

    char *active_model = g_settings_get_string(self->settings, "active-model");
    if(g_str_equal(model, active_model)) {
        gtk_check_button_set_active(button, true);
    }

    g_free(active_model);

    gpointer quark = (gpointer)g_quark_from_string(model);

    g_object_set_data(G_OBJECT(button), "lcap-model-path", quark);
    g_signal_connect(button, "toggled", G_CALLBACK(on_model_selected), self);

    g_object_set_data(G_OBJECT(delete_button), "lcap-model-path", quark);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_model_deleted), self);
}

static void init_models_page(LiveCaptionsSettings *self) {
    gchar **models = g_settings_get_strv(self->settings, "installed-models");

    for(int i=0; models[i] != NULL; i++){
        gchar *model = models[i];
        insert_model_to_list(self, model);
    }

    g_strfreev(models);
}

static void add_new_model(LiveCaptionsSettings *self, gchar *model) {
    gchar **existing_models = g_settings_get_strv(self->settings, "installed-models");

    if(g_strv_contains((const gchar * const *)existing_models, model)) return;

    GStrvBuilder *builder = g_strv_builder_new();
    g_strv_builder_addv(builder, (const gchar **)existing_models);
    g_strv_builder_add(builder, model);

    GStrv result = g_strv_builder_end(builder);

    g_settings_set_strv(self->settings, "installed-models", (const gchar * const *)result);

    g_strfreev(result);
    g_strv_builder_unref(builder);
}

static void model_load_failsafe(LiveCaptionsSettings *self, bool load_default) {
    GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL;
    GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(self),
                                        flags,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        "Failed to load model!");
    
    gtk_window_present(GTK_WINDOW(dialog));

    g_signal_connect(dialog, "response",
                    G_CALLBACK (gtk_window_destroy),
                    NULL);
    
    char *prev_model;
    if(load_default) {
        prev_model = GET_MODEL_PATH();
    }else{
        prev_model = g_settings_get_string(self->settings, "active-model");
    }

    asr_thread_update_model(self->application->asr, prev_model);

    if(load_default) gtk_check_button_set_active(self->radio_button_1, true);
}

static void on_add_model_response(GtkNativeDialog *native,
                                  int        response,
                                  LiveCaptionsSettings *self)
{
    if(response == GTK_RESPONSE_ACCEPT){
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);

        g_autoptr(GFile) file = gtk_file_chooser_get_file(chooser);
        
        char *model = g_file_get_path(file);


        if(!asr_thread_update_model(self->application->asr, model)){
            model_load_failsafe(self, false);

            g_free(model);
            g_object_unref(native);
            return;
        }
        
        g_settings_set_string(self->settings, "active-model", model);


        insert_model_to_list(self, model);
        add_new_model(self, model);

        g_free(model);
    }

    g_object_unref(native);
}

static void livecaptions_settings_init(LiveCaptionsSettings *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    adw_action_row_set_activatable_widget(self->font_button_ar, GTK_WIDGET(self->font_button));

    self->settings = g_settings_new("net.sapples.LiveCaptions");

    g_settings_bind(self->settings, "text-uppercase", self->text_upper_switch, "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "fade-text", self->fade_text_switch, "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "filter-profanity", self->filter_profanity_switch, "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "filter-slurs", self->filter_slurs_switch, "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "save-history", self->save_history_switch, "active", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "line-width", self->line_width_adjustment, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "window-transparency", self->window_transparency_adjustment, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "text-stream-active", self->text_stream_switch, "active", G_SETTINGS_BIND_DEFAULT);

    g_settings_bind(self->settings, "font-name", self->font_button, "font", G_SETTINGS_BIND_DEFAULT);

    gtk_scale_add_mark(self->line_width_scale, 50.0, GTK_POS_TOP, NULL);
    gtk_scale_add_mark(self->window_transparency_scale, 0.25, GTK_POS_TOP, NULL);

    char benchmark_result[32];
    double benchmark_result_v = g_settings_get_double(self->settings, "benchmark");
    sprintf(benchmark_result, "%.2f", (float)benchmark_result_v);
    gtk_label_set_text(self->benchmark_label, benchmark_result);

    if(is_keep_above_supported(GTK_WINDOW(self))) {
        g_settings_bind(self->settings, "keep-on-top", self->keep_above_switch, "active", G_SETTINGS_BIND_DEFAULT);
        gtk_widget_set_sensitive(GTK_WIDGET(self->keep_above_switch), true);

        g_idle_add(deferred_update_keep_above, self);

        gtk_label_set_label(self->keep_above_instructions, "");
    } else {
        adw_action_row_set_subtitle(ADW_ACTION_ROW(self->keep_above_switch), _("Your compositor does not support this setting. Read below for manual instructions"));
        gtk_widget_set_sensitive(GTK_WIDGET(self->keep_above_switch), false);
        adw_switch_row_set_active(self->keep_above_switch, false);

        const char *always_on_top_text = get_always_on_top_tip_text();
        gtk_label_set_label(self->keep_above_instructions, always_on_top_text);
    }

    init_models_page(self);

    g_settings_bind(self->settings, "openai-key", self->openai_key_entry, "text", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "openai-url", self->openai_url_entry, "text", G_SETTINGS_BIND_DEFAULT);
}
