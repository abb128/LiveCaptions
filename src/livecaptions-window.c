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
#include "april_api.h"
#include "audiocap.h"
#include "window-helper.h"
#include "history.h"


G_DEFINE_TYPE(LiveCaptionsWindow, livecaptions_window, GTK_TYPE_APPLICATION_WINDOW)

static void on_hover_word_changed(G_GNUC_UNUSED LiveCaptionsTranslationView *view,
                                  const gchar *word,
                                  gpointer user_data)
{
    LiveCaptionsWindow *self = LIVECAPTIONS_WINDOW(user_data);
    GtkApplication *application = gtk_window_get_application(GTK_WINDOW(self));
    if(application != NULL && LIVECAPTIONS_IS_APPLICATION(application)) {
        livecaptions_application_request_hover_translation(
            LIVECAPTIONS_APPLICATION(application),
            word);
    }
}

static gboolean on_window_close_request(GtkWindow *window,
                                        G_GNUC_UNUSED gpointer user_data)
{
    GtkApplication *gtk_application = gtk_window_get_application(window);
    if(gtk_application == NULL || !LIVECAPTIONS_IS_APPLICATION(gtk_application))
        return FALSE;

    LiveCaptionsApplication *application = LIVECAPTIONS_APPLICATION(gtk_application);
    LiveCaptionsWindow *self = LIVECAPTIONS_WINDOW(window);
    livecaptions_translation_view_clear(self->translation_view);
    application->window = NULL;
    g_application_quit(G_APPLICATION(application));
    return FALSE;
}

static void livecaptions_window_dispose(GObject *object)
{
    LiveCaptionsWindow *self = LIVECAPTIONS_WINDOW(object);

    GtkApplication *application = gtk_window_get_application(GTK_WINDOW(self));
    if(application != NULL && LIVECAPTIONS_IS_APPLICATION(application)) {
        LiveCaptionsApplication *livecaptions =
            LIVECAPTIONS_APPLICATION(application);
        if(livecaptions->window == self)
            livecaptions->window = NULL;
    }

    g_clear_object(&self->translation_view);
    G_OBJECT_CLASS(livecaptions_window_parent_class)->dispose(object);
}

static void livecaptions_window_class_init (LiveCaptionsWindowClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->dispose = livecaptions_window_dispose;

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-window.ui");
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, main);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, side_box);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, side_box_tiny);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, mic_button);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, label);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, translation_separator);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, translation_box);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, translation_label_first);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, translation_label_second);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, too_slow_warning);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, slow_warning);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, slowest_warning);
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

    int text_line_height = height / PANGO_SCALE;
    height = text_line_height * 2 + 2;
    width  = (width / PANGO_SCALE);
    /* Keep a stable width so changing translations cannot resize the window. */
    int translation_width = width + width / 3;
    change_button_layout(self, height);

    gtk_widget_set_size_request(GTK_WIDGET(self->label), width, height);
    gtk_widget_set_size_request(GTK_WIDGET(self->translation_separator), translation_width, 2);
    /* Reserve exactly one translated row for each source caption row. */
    gtk_widget_set_size_request(GTK_WIDGET(self->translation_label_first),
                                translation_width,
                                text_line_height);
    gtk_widget_set_size_request(GTK_WIDGET(self->translation_label_second),
                                translation_width,
                                text_line_height);
    gtk_widget_set_size_request(GTK_WIDGET(self->translation_box),
                                translation_width,
                                text_line_height * 2);

    self->max_text_width = width;
    self->font_layout = layout;
    self->font_layout_counter++;
}

static void update_font(LiveCaptionsWindow *self) {
    PangoFontDescription *desc = pango_font_description_from_string(
        g_settings_get_string(self->settings, "font-name"));

    PangoAttribute *attr_font = pango_attr_font_desc_new(desc);
    PangoAttrList *attr = gtk_label_get_attributes(self->label);
    if(attr == NULL)
        attr = pango_attr_list_new();
    pango_attr_list_change(attr, attr_font);

    gtk_label_set_attributes(self->label, attr);
    /* Match the translated rows to the existing caption font. */
    gtk_label_set_attributes(self->translation_label_first, attr);
    gtk_label_set_attributes(self->translation_label_second, attr);
    pango_font_description_free(desc);

    update_line_width(self);
}

static void update_translation_visibility(LiveCaptionsWindow *self) {
    gboolean enabled = g_settings_get_boolean(self->settings, "show-dual-subtitles");
    const gchar *first = gtk_label_get_text(self->translation_label_first);
    const gchar *second = gtk_label_get_text(self->translation_label_second);
    gboolean has_first = first != NULL && first[0] != '\0';
    gboolean has_second = second != NULL && second[0] != '\0';
    gboolean has_text = has_first || has_second;

    /* Keep both row slots allocated to prevent the window jumping as text changes. */
    gtk_widget_set_visible(GTK_WIDGET(self->translation_separator), enabled);
    gtk_widget_set_visible(GTK_WIDGET(self->translation_box), enabled);
    gtk_widget_set_opacity(GTK_WIDGET(self->translation_separator), has_text ? 0.88 : 0.0);
    gtk_widget_set_opacity(GTK_WIDGET(self->translation_label_first), has_first ? 1.0 : 0.0);
    gtk_widget_set_opacity(GTK_WIDGET(self->translation_label_second), has_second ? 1.0 : 0.0);
}

static void update_translation_text_opacity(LiveCaptionsWindow *self) {
    double opacity = CLAMP(
        g_settings_get_double(self->settings, "translation-text-opacity"),
        0.0,
        1.0
    );
    gtk_widget_set_opacity(GTK_WIDGET(self->translation_box), opacity);
    if(self->translation_view != NULL)
        livecaptions_translation_view_set_opacity(self->translation_view, opacity);
}

static void update_window_transparency(LiveCaptionsWindow *self) {
    bool use_transparency = g_settings_get_double(self->settings, "window-transparency") > 0.01;

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

static void update_keep_above(LiveCaptionsWindow *self) {
    gboolean keep_above = g_settings_get_boolean(self->settings, "keep-on-top");
    if(!set_window_keep_above(GTK_WINDOW(self), keep_above)){
        printf("Failed to set keep above\n");
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
    }else if(g_str_equal(key, "keep-on-top")) {
        update_keep_above(self);
    }else if(g_str_equal(key, "show-dual-subtitles")) {
        update_translation_visibility(self);
    }else if(g_str_equal(key, "translation-text-opacity")) {
        update_translation_text_opacity(self);
    }
}


static gboolean deferred_update_keep_above(void *userdata) {
    LiveCaptionsWindow *self = userdata;

    update_keep_above(self);

    return G_SOURCE_REMOVE;
}

static gboolean show_relevant_slow_warning(void *userdata) {
    LiveCaptionsWindow *self = userdata;

    GtkApplication *curr_app = gtk_window_get_application(GTK_WINDOW(self));
    LiveCaptionsApplication *app = LIVECAPTIONS_APPLICATION(curr_app);

    asr_thread asr = app->asr;

    if(asr_thread_is_errored(asr)){
        gtk_label_set_text(self->label, "[Model Error]");
        self->was_errored = true;
        return G_SOURCE_CONTINUE;
    }else if(self->was_errored) {
        self->was_errored = false;
        gtk_label_set_text(self->label, "");
    }

    AprilASRSession session = (AprilASRSession)asr_thread_get_session(asr);
    if(session == NULL) return G_SOURCE_CONTINUE;

    float speedup = aas_realtime_get_speedup(session);

    if(speedup <= 1.1) {
        gtk_widget_set_visible(GTK_WIDGET(self->slow_warning), false);
        gtk_widget_set_visible(GTK_WIDGET(self->slowest_warning), false);
        gtk_widget_set_visible(GTK_WIDGET(self->too_slow_warning), false);
    }else if(speedup <= 1.666) {
        gtk_widget_set_visible(GTK_WIDGET(self->slow_warning), true);
        gtk_widget_set_visible(GTK_WIDGET(self->slowest_warning), false);
        gtk_widget_set_visible(GTK_WIDGET(self->too_slow_warning), false);
    }else if(speedup <= 2.33){
        gtk_widget_set_visible(GTK_WIDGET(self->slow_warning), false);
        gtk_widget_set_visible(GTK_WIDGET(self->slowest_warning), true);
        gtk_widget_set_visible(GTK_WIDGET(self->too_slow_warning), false);
    }else{
        gtk_widget_set_visible(GTK_WIDGET(self->slow_warning), false);
        gtk_widget_set_visible(GTK_WIDGET(self->slowest_warning), false);
        gtk_widget_set_visible(GTK_WIDGET(self->too_slow_warning), true);
    }

    return G_SOURCE_CONTINUE;
}

static void livecaptions_window_init(LiveCaptionsWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    self->settings = g_settings_new("net.sapples.LiveCaptions");
    self->translation_view =
        livecaptions_translation_view_new(self->label, self->settings);
    g_signal_connect_object(self->translation_view,
                            "hover-word-changed",
                            G_CALLBACK(on_hover_word_changed),
                            self,
                            0);
    g_signal_connect(self, "close-request", G_CALLBACK(on_window_close_request), NULL);

    self->css_provider = gtk_css_provider_new();
    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(self));
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(self->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(self->settings, "changed", G_CALLBACK(on_settings_change), self);
    
    g_settings_bind(self->settings, "microphone", self->mic_button, "active", G_SETTINGS_BIND_DEFAULT);

    self->font_layout = NULL;
    self->font_layout_counter = 0;

    update_font(self);
    update_window_transparency(self);
    update_translation_visibility(self);
    update_translation_text_opacity(self);

    self->slow_warning_shown = false;
    self->was_errored = false;

    g_idle_add(deferred_update_keep_above, self);
    g_timeout_add_seconds(5, show_relevant_slow_warning, self);

    // GTK adds solid-csd class when the compositor does not support window shadows
    // This adds a very ugly thick border, so we remove it
    gtk_widget_remove_css_class(GTK_WIDGET(self), "solid-csd");
}

void livecaptions_window_set_translation(LiveCaptionsWindow *self, const gchar *text) {
    g_return_if_fail(LIVECAPTIONS_IS_WINDOW(self));

    g_auto(GStrv) lines = g_strsplit(text != NULL ? text : "", "\n", 3);
    gtk_label_set_text(self->translation_label_first, lines[0] != NULL ? lines[0] : "");
    gtk_label_set_text(self->translation_label_second, lines[1] != NULL ? lines[1] : "");
    update_translation_visibility(self);
}

void livecaptions_window_set_hover_translation(LiveCaptionsWindow *self, const gchar *text) {
    g_return_if_fail(LIVECAPTIONS_IS_WINDOW(self));
    livecaptions_translation_view_set_translation(self->translation_view, text);
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
