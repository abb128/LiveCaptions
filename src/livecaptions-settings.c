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


static void change_font(LiveCaptionsSettings *self) {
    
}

static void livecaptions_settings_class_init(LiveCaptionsSettingsClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-settings.ui");
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, subpage1);
    gtk_widget_class_bind_template_child (widget_class, LiveCaptionsSettings, subpage2);

    gtk_widget_class_bind_template_callback (widget_class, change_font);
    gtk_widget_class_bind_template_callback (widget_class, return_to_preferences_cb);
    gtk_widget_class_bind_template_callback (widget_class, subpage1_activated_cb);
    gtk_widget_class_bind_template_callback (widget_class, subpage2_activated_cb);

    gtk_widget_class_install_action (widget_class, "toast.show", NULL, (GtkWidgetActionActivateFunc) toast_show_cb);
}

static void livecaptions_settings_init(LiveCaptionsSettings *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
}
