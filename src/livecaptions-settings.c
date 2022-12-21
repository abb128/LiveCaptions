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
#include "livecaptions-config.h"
#include "livecaptions-settings.h"
#include "livecaptions-application.h"


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

    adw_about_window_add_acknowledgement_section(ADW_ABOUT_WINDOW(about),
                                                 _("Special thanks to"),
                                                 special_thanks);

    gtk_window_present(GTK_WINDOW(about));
}


static void report_cb(LiveCaptionsSettings *self) {
    gtk_show_uri(
        GTK_WINDOW(self),
        "https://github.com/abb128/LiveCaptions/issues/1",
        GDK_CURRENT_TIME
    );
}

static void livecaptions_settings_class_init(LiveCaptionsSettingsClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-settings.ui");

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, font_button);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, font_button_ar);
    
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, text_upper_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, text_upper_switch_ar);
    
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, fade_text_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, fade_text_switch_ar);

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, filter_profanity_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, filter_profanity_switch_ar);
    //gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, filter_slurs_switch);
    //
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, transparent_window_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, transparent_window_switch_ar);

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, line_width_scale);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, line_width_adjustment);

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, benchmark_label);

    gtk_widget_class_bind_template_callback (widget_class, report_cb);
    gtk_widget_class_bind_template_callback (widget_class, about_cb);
    gtk_widget_class_bind_template_callback (widget_class, rerun_benchmark_cb);
}

static void livecaptions_settings_init(LiveCaptionsSettings *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    adw_action_row_set_activatable_widget(self->font_button_ar, GTK_WIDGET(self->font_button));
    adw_action_row_set_activatable_widget(self->text_upper_switch_ar, GTK_WIDGET(self->text_upper_switch));
    adw_action_row_set_activatable_widget(self->fade_text_switch_ar, GTK_WIDGET(self->fade_text_switch));
    adw_action_row_set_activatable_widget(self->filter_profanity_switch_ar, GTK_WIDGET(self->filter_profanity_switch));
    adw_action_row_set_activatable_widget(self->transparent_window_switch_ar, GTK_WIDGET(self->transparent_window_switch));

    self->settings = g_settings_new("net.sapples.LiveCaptions");

    g_settings_bind(self->settings, "text-uppercase", self->text_upper_switch, "state", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "fade-text", self->fade_text_switch, "state", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "filter-profanity", self->filter_profanity_switch, "state", G_SETTINGS_BIND_DEFAULT);
    //g_settings_bind(self->settings, "filter-slurs", self->filter_slurs_switch, "state", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "transparent-window", self->transparent_window_switch, "state", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "line-width", self->line_width_adjustment, "value", G_SETTINGS_BIND_DEFAULT);

    g_settings_bind(self->settings, "font-name", self->font_button, "font", G_SETTINGS_BIND_DEFAULT);

    gtk_scale_add_mark(self->line_width_scale, 50.0, GTK_POS_TOP, NULL);

    char benchmark_result[32];
    double benchmark_result_v = g_settings_get_double(self->settings, "benchmark");
    sprintf(benchmark_result, "%.2f", (float)benchmark_result_v);
    gtk_label_set_text(self->benchmark_label, benchmark_result);
}
