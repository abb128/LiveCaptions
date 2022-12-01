/* livecaptions-window.c
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

#include "livecaptions-config.h"
#include "livecaptions-settings.h"
#include "livecaptions-application.h"

G_DEFINE_TYPE(LiveCaptionsSettings, livecaptions_settings, ADW_TYPE_PREFERENCES_WINDOW)

static void
return_to_preferences_cb (LiveCaptionsSettings *self)
{
  adw_preferences_window_close_subpage (ADW_PREFERENCES_WINDOW (self));
}


static void
subpage1_activated_cb (LiveCaptionsSettings *self)
{
  adw_preferences_window_present_subpage (ADW_PREFERENCES_WINDOW (self), self->subpage1);
}

static void
subpage2_activated_cb (LiveCaptionsSettings *self)
{
  adw_preferences_window_present_subpage (ADW_PREFERENCES_WINDOW (self), self->subpage2);
}

static void
toast_show_cb (AdwPreferencesWindow *window)
{
  adw_preferences_window_add_toast (window, adw_toast_new ("Example Toast"));
}


static void issue_tracker_cb(LiveCaptionsSettings *self){
    system("xdg-open https://github.com/abb128/LiveCaptions/issues");
}

static void git_cb(LiveCaptionsSettings *self){
    system("xdg-open https://github.com/abb128/LiveCaptions");
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

    //const char *release_notes = "\
    //    <p>\
    //    This release adds the following features:\
    //    </p>\
    //    <ul>\
    //    <li>Added a way to export fonts.</li>\
    //    <li>Better support for <code>monospace</code> fonts.</li>\
    //    <li>Added a way to preview <em>italic</em> text.</li>\
    //    <li>Bug fixes and performance improvements.</li>\
    //    <li>Translation updates.</li>\
    //    </ul>\
    //";

    about =
        g_object_new (ADW_TYPE_ABOUT_WINDOW,
                    "transient-for", root,
                    "application-icon", "net.sapples.LiveCaptions",
                    "application-name", _("Live Captions"),
                    "developer-name", _("abb128"),
                    "version", "0.0.1",
                    //"release-notes-version", "1.2.0",
                    //"release-notes", release_notes,
                    "comments", _("Live Captions is an application for the Linux Desktop that captions desktop audio."),
                    "website", "https://github.com/abb128/LiveCaptions",
                    "issue-url", "https://github.com/abb128/LiveCaptions/issues",
                    //"support-url", "https://example.org",
                    "copyright", "© 2022 abb128",
                    "license-type", GTK_LICENSE_GPL_3_0,
                    "developers", developers,
                    //"artists", artists,
                    "translator-credits", _("translator-credits"),
                    NULL);

    //adw_about_window_add_link (ADW_ABOUT_WINDOW (about),
    //                            _("_Documentation"),
    //                            "https://gnome.pages.gitlab.gnome.org/libadwaita/doc/main/class.AboutWindow.html");

    adw_about_window_add_legal_section (ADW_ABOUT_WINDOW (about),
                                        _("Model"),
                                        NULL,
                                        GTK_LICENSE_CUSTOM,
                                        "The ASR model was originally trained by Fangjun Kuang (@csukuangfj), and has been finetuned on extra data.");

    adw_about_window_add_acknowledgement_section (ADW_ABOUT_WINDOW (about),
                                                    _("Special thanks to"),
                                                    special_thanks);

    gtk_window_present (GTK_WINDOW (about));
}


static void change_font(LiveCaptionsSettings *self) {

}

static void livecaptions_settings_class_init(LiveCaptionsSettingsClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-settings.ui");
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, subpage1);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, subpage2);

    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, font_button);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, text_upper_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, fade_text_switch);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, filter_profanity_switch);


    gtk_widget_class_bind_template_callback (widget_class, change_font);
    gtk_widget_class_bind_template_callback (widget_class, return_to_preferences_cb);
    gtk_widget_class_bind_template_callback (widget_class, issue_tracker_cb);
    gtk_widget_class_bind_template_callback (widget_class, git_cb);
    gtk_widget_class_bind_template_callback (widget_class, about_cb);
    gtk_widget_class_bind_template_callback (widget_class, subpage1_activated_cb);
    gtk_widget_class_bind_template_callback (widget_class, subpage2_activated_cb);

    gtk_widget_class_install_action (widget_class, "toast.show", NULL, (GtkWidgetActionActivateFunc) toast_show_cb);
}

static void livecaptions_settings_init(LiveCaptionsSettings *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    self->settings = g_settings_new("net.sapples.LiveCaptions");

    g_settings_bind(self->settings, "text-uppercase", self->text_upper_switch, "state", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "fade-text", self->fade_text_switch, "state", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(self->settings, "filter-profanity", self->filter_profanity_switch, "state", G_SETTINGS_BIND_DEFAULT);

}
