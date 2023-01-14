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
#include "window-helper.h"


G_DEFINE_TYPE(LiveCaptionsWindow, livecaptions_window, GTK_TYPE_APPLICATION_WINDOW)

static void livecaptions_window_class_init (LiveCaptionsWindowClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-window.ui");
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, main);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, side_box);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, side_box_tiny);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, mic_button);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, label);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, too_slow_warning);
}

static void change_button_layout(LiveCaptionsWindow *self, gint text_height){
    int button_height = 29;

    if(text_height > (2 * button_height)) {
        gtk_widget_set_visible(GTK_WIDGET(self->side_box), true);
        gtk_widget_set_visible(GTK_WIDGET(self->side_box_tiny), false);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(self->side_box_tiny), true);
        gtk_widget_set_visible(GTK_WIDGET(self->side_box), false);
    }
}

const char LINE_WIDTH_TEXT_TEMPLATE[] = "This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.";
static void update_line_width(LiveCaptionsWindow *self){
    if(self->font_layout != NULL) g_object_unref(self->font_layout);

    int preferred_width = g_settings_get_int(self->settings, "line-width");
    size_t text_len = sizeof(LINE_WIDTH_TEXT_TEMPLATE);
    if(preferred_width < text_len) text_len = preferred_width;

    int width, height;
    PangoLayout *layout = gtk_label_get_layout(self->label);
    layout = pango_layout_copy(layout);

    pango_layout_set_width(layout, -1);
    pango_layout_set_text(layout, LINE_WIDTH_TEXT_TEMPLATE, text_len);
    pango_layout_get_size(layout, &width, &height);

    height = (height / PANGO_SCALE) * 2 + 2;
    width  = (width / PANGO_SCALE);
    change_button_layout(self, height);

    gtk_widget_set_size_request(GTK_WIDGET(self->label), width, height);

    self->max_text_width = width;
    self->font_layout = layout;
    self->font_layout_counter++;
}

static void update_font(LiveCaptionsWindow *self) {
    PangoFontDescription *desc = pango_font_description_from_string(g_settings_get_string(self->settings, "font-name"));

    PangoAttribute *attr_font = pango_attr_font_desc_new(desc);

    PangoAttrList *attr = gtk_label_get_attributes(self->label);
    if(attr == NULL){
        attr = pango_attr_list_new();
    }
    pango_attr_list_change(attr, attr_font);

    gtk_label_set_attributes(self->label, attr);

    pango_font_description_free(desc);

    update_line_width(self);
}

static void update_window_transparency(LiveCaptionsWindow *self) {
    bool use_transparency = true;// g_settings_get_boolean(self->settings, "transparent-window");

    if(use_transparency){
        gtk_widget_add_css_class(GTK_WIDGET(self), "transparent-mode");

        int transparency = (int)((1.0 - g_settings_get_double(self->settings, "window-transparency")) * 255.0);

        char css_data[256];
        snprintf(css_data, 256, ".transparent-mode {\nbackground-color: #000000%02X;\n}", transparency);
        gtk_css_provider_load_from_data(self->css_provider, css_data, -1);
    }else{
        gtk_widget_remove_css_class(GTK_WIDGET(self), "transparent-mode");
    }
}

static void on_settings_change(G_GNUC_UNUSED GSettings *settings,
                               char      *key,
                               gpointer   user_data){

    LiveCaptionsWindow *self = user_data;
    if(g_str_equal(key, "font-name")) {
        update_font(self);
    }else if(g_str_equal(key, "line-width")) {
        update_line_width(self);
    }else if(g_str_equal(key, "transparent-window") || g_str_equal(key, "window-transparency")) {
        update_window_transparency(self);
    }
}


static gboolean livecaptions_always_on_top(void *userdata) {
    LiveCaptionsWindow *self = userdata;

    printf("set window: %s\n", set_window_keep_above(GTK_WINDOW(self), true) ? "true" : "false");

    return G_SOURCE_REMOVE;
}

static void livecaptions_window_init(LiveCaptionsWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    self->settings = g_settings_new("net.sapples.LiveCaptions");

    self->css_provider = gtk_css_provider_new();
    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(self));
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(self->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(self->settings, "changed", G_CALLBACK(on_settings_change), self);
    
    g_settings_bind(self->settings, "microphone", self->mic_button, "active", G_SETTINGS_BIND_DEFAULT);

    self->font_layout = NULL;
    self->font_layout_counter = 0;

    update_font(self);
    update_window_transparency(self);

    self->slow_warning_shown = false;

    g_idle_add(livecaptions_always_on_top, self);
}

static gboolean hide_slow_warning_after_some_time(void *userdata) {
    LiveCaptionsWindow *self = userdata;

    time_t current_time = time(NULL);

    if(difftime(current_time, self->slow_time) > 4.0) {
        self->slow_warning_shown = false;
        gtk_widget_set_visible(GTK_WIDGET(self->too_slow_warning), false);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

void livecaptions_window_warn_slow(LiveCaptionsWindow *self) {
    self->slow_time = time(NULL);
    if(self->slow_warning_shown) return;

    gtk_widget_set_visible(GTK_WIDGET(self->too_slow_warning), true);
    self->slow_warning_shown = true;

    g_timeout_add_seconds(1, hide_slow_warning_after_some_time, self);
}
