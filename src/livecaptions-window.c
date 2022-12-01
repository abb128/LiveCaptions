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
#include "livecaptions-window.h"
#include "livecaptions-application.h"
#include "audiocap.h"


G_DEFINE_TYPE(LiveCaptionsWindow, livecaptions_window, GTK_TYPE_APPLICATION_WINDOW)

static void livecaptions_window_class_init (LiveCaptionsWindowClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-window.ui");
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, main);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, side_box);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, mic_button);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, label);
}

static void change_orientable_orientation(LiveCaptionsWindow *self, gint font_height){
    int text_height = 2 * font_height + 2;
    int button_height = 16;

    if(text_height > 2 * button_height) {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(self->side_box), GTK_ORIENTATION_VERTICAL);
    } else {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(self->side_box), GTK_ORIENTATION_HORIZONTAL);
    }
}


static void update_font(LiveCaptionsWindow *self) {
    PangoFontDescription *desc = pango_font_description_from_string(g_settings_get_string(self->settings, "font-name"));
    gint font_size = pango_font_description_get_size(desc) / PANGO_SCALE;

    PangoAttribute *attr_font = pango_attr_font_desc_new(desc);

    PangoAttrList *attr = gtk_label_get_attributes(self->label);
    if(attr == NULL){
        attr = pango_attr_list_new();
    }
    pango_attr_list_change(attr, attr_font);

    gtk_label_set_attributes(self->label, attr);

    change_orientable_orientation(self, font_size);

    // pango_attr_list_unref(attr); // ?
    pango_font_description_free(desc);


    gtk_label_set_width_chars(self->label, 100 * font_size / 24);
}

static void on_settings_change(GSettings *settings,
                               char      *key,
                               gpointer   user_data){

    if(!g_str_equal(key, "font-name")) return;

    LiveCaptionsWindow *self = user_data;

    update_font(self);
}


static void livecaptions_window_init (LiveCaptionsWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    self->settings = g_settings_new("net.sapples.LiveCaptions");

    gtk_window_set_titlebar(GTK_WINDOW(self), GTK_WIDGET(self->main));

    g_signal_connect(self->settings, "changed", G_CALLBACK(on_settings_change), self);
    
    g_settings_bind(self->settings, "microphone", self->mic_button, "active", G_SETTINGS_BIND_DEFAULT);


    update_font(self);
}
