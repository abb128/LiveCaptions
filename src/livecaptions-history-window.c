/* livecaptions-history-window.c
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

#include <ctype.h>
#include <glib/gi18n.h>
#include <april_api.h>
#include <adwaita.h>
#include "livecaptions-history-window.h"
#include "history.h"
#include "profanity-filter.h"
#include "common.h"
#include "window-helper.h"
#include "line-gen.h"
#include "openai.h"

G_DEFINE_TYPE(LiveCaptionsHistoryWindow, livecaptions_history_window, GTK_TYPE_WINDOW)


static gboolean close_self_window(gpointer userdata) {
    LiveCaptionsHistoryWindow *self = LIVECAPTIONS_HISTORY_WINDOW(userdata);

    gtk_window_close(GTK_WINDOW(self));
    //gtk_window_destroy(GTK_WINDOW(self));

    return G_SOURCE_REMOVE;
}

static gboolean force_bottom(gpointer userdata) {
    LiveCaptionsHistoryWindow *self = LIVECAPTIONS_HISTORY_WINDOW(userdata);

    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(self->scroll));
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));

    return G_SOURCE_REMOVE;
}

static void add_text(LiveCaptionsHistoryWindow *self, char *text, bool is_text) {
    GtkWidget *label = gtk_label_new(text);

    if(is_text){
        PangoFontDescription *desc = pango_font_description_from_string(g_settings_get_string(self->settings, "font-name"));
        PangoAttribute *attr_font = pango_attr_font_desc_new(desc);
        PangoAttrList *attr = gtk_label_get_attributes(GTK_LABEL(label));
        if(attr == NULL){
            attr = pango_attr_list_new();
        }
        pango_attr_list_change(attr, attr_font);
        gtk_label_set_attributes(GTK_LABEL(label), attr);
        pango_font_description_free(desc);
    }

    gtk_label_set_selectable(GTK_LABEL(label), true);
    gtk_label_set_wrap(GTK_LABEL(label), true);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, is_text ? "history-label" : "timestamp-label");

    gtk_widget_set_hexpand(label, true);
    gtk_widget_set_halign(label, GTK_ALIGN_FILL);

    gtk_box_prepend(self->main_box, label);
}

static void add_time(LiveCaptionsHistoryWindow *self, time_t timestamp, bool date) {
    char text[64];

    struct tm *tm = localtime(&timestamp);
    strftime(text, 64, date ? "\n\nStart of session %F | %H:%M" : "%H:%M:%S", tm);

    add_text(self, text, false);
}

static void add_session(LiveCaptionsHistoryWindow *self, const struct history_session *session){
    if(session->entries_count == 0) return;

    bool use_lowercase = !g_settings_get_boolean(self->settings, "text-uppercase");
    GString *string = g_string_new(NULL);

    bool filter_slurs = g_settings_get_boolean(self->settings, "filter-slurs");
    bool filter_profanity = g_settings_get_boolean(self->settings, "filter-profanity");

    FilterMode filter_mode = filter_profanity ? FILTER_PROFANITY : (filter_slurs ? FILTER_SLURS : FILTER_NONE);
    struct token_capitalizer tcap;
    token_capitalizer_init(&tcap);

    for(size_t i_1=0; i_1<session->entries_count; i_1++){
        size_t i = session->entries_count - i_1 - 1;

        const struct history_entry *entry = &session->entries[i];

        if(entry->tokens_count == 0){
            // Silence
            if((i + 1) >= session->entries_count) continue;
            const struct history_entry *next_entry = &session->entries[i+1];

            add_text(self, string->str, true);
            add_time(self, next_entry->timestamp, false);

            g_string_truncate(string, 0);
        } else {
            GString *entry_text = g_string_new(NULL);

            if(entry->tokens[0].flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT) {
                tcap.previous_was_period = true;
            }

            for(size_t j=0; j<entry->tokens_count;) {
                size_t skipahead = 1;
                const char *token = entry->tokens[j].token;

                if((filter_mode > FILTER_NONE) && (entry->tokens[j].flags & APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT)) {
                    size_t skip = get_filter_skip_history(entry->tokens, j, entry->tokens_count, filter_mode);
                    if(skip > 0) {
                        skipahead = skip;
                        token = SWEAR_REPLACEMENT;
                    }
                }

                if((j == 0) && (*token == ' ')) token++;

                bool should_be_capitalized = false;
                if((j+skipahead) < entry->tokens_count){
                    should_be_capitalized = use_lowercase && token_capitalizer_next(&tcap, token, entry->tokens[j].flags, entry->tokens[j+skipahead].token, entry->tokens[j+skipahead].flags);
                }else{
                    should_be_capitalized = use_lowercase && token_capitalizer_next(&tcap, token, entry->tokens[j].flags, NULL, 0);
                }

                if(use_lowercase){
                    const char *p = token;
                    gunichar c;
                    while (*p) {
                        c = g_utf8_get_char_validated(p, -1);
                        if(c == ((gunichar)-2)) {
                            printf("gunichar -2 \n");
                            break;
                        }else if(c == ((gunichar)-1)) {
                            printf("gunichar -1 \n");
                            break;
                        }

                        c = g_unichar_tolower(c);

                        if(should_be_capitalized){
                            gunichar c1 = g_unichar_toupper(c);
                            if(c != c1){
                                c = c1;
                                should_be_capitalized = false;
                            }
                        }

                        g_string_append_unichar(entry_text, c);

                        p = g_utf8_next_char(p);
                    }
                }else{
                    g_string_append(entry_text, token);
                }

                j += skipahead;
            }

            g_string_append_c(entry_text, '\n');
            g_string_prepend(string, entry_text->str);

            g_string_free(entry_text, true);
        }
    }

    add_text(self, string->str, true);
    add_time(self, session->entries[0].timestamp, true);
}

static void load_to(LiveCaptionsHistoryWindow *self, size_t idx){
    for(size_t i_1=0; i_1<1; i_1++){
        const struct history_session *session = get_history_session(idx - i_1 - 1);
        if(session == NULL) break;

        // TODO: text fading?
        add_session(self, session);
    }
}

static void load_more_cb(LiveCaptionsHistoryWindow *self) {
    // gets very laggy
    load_to(self, ++self->session_load);
}


static void on_save_response(GtkNativeDialog *native,
                             int        response,
                             LiveCaptionsHistoryWindow *self)
{
    if(response == GTK_RESPONSE_ACCEPT){
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);

        g_autoptr(GFile) file = gtk_file_chooser_get_file(chooser);
        
        char *path = g_file_get_path(file);

        export_history_as_text(path);

        char *uri = g_file_get_uri(file);

        gtk_show_uri(GTK_WINDOW(self), uri, GDK_CURRENT_TIME);

        g_free(path);
        g_free(uri);
    }

    g_object_unref(native);
}

static void export_cb(LiveCaptionsHistoryWindow *self) {
    GtkFileChooserNative *native;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;

    native = gtk_file_chooser_native_new("Export History",
                                         GTK_WINDOW(self),
                                         action,
                                         _("_Export"),
                                         _("_Cancel"));

    chooser = GTK_FILE_CHOOSER(native);

    gtk_file_chooser_set_current_name(chooser, _("Live Captions History.txt"));

    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));

    g_signal_connect(native, "response",
                     G_CALLBACK (on_save_response),
                     self);
}

// The window needs to be kept on top if the main window is kept on top,
// otherwise it will appear under the main window which is not ideal
static gboolean deferred_update_keep_above(void *userdata) {
    LiveCaptionsHistoryWindow *self = userdata;

    set_window_keep_above(GTK_WINDOW(self), g_settings_get_boolean(self->settings, "keep-on-top"));

    return G_SOURCE_REMOVE;
}


static void refresh_cb(LiveCaptionsHistoryWindow *self) {
    GtkWidget *childs = gtk_widget_get_first_child(GTK_WIDGET(self->main_box));
    while (childs != NULL) {
        gtk_box_remove(self->main_box, childs);
        childs = gtk_widget_get_first_child(GTK_WIDGET(self->main_box));
    }

    self->session_load = 0;
    load_to(self, ++self->session_load);

    g_idle_add(force_bottom, self);
}

static void message_cb(AdwMessageDialog *dialog, gchar *response, gpointer userdata){
    if(g_str_equal(response, "delete")){
        erase_all_history();

        LiveCaptionsHistoryWindow *self = LIVECAPTIONS_HISTORY_WINDOW(userdata);
        refresh_cb(self);
        //g_idle_add(close_self_window, userdata);
    }
}

static void warn_deletion_cb(LiveCaptionsHistoryWindow *self){
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    GtkWidget *dialog;

    dialog = adw_message_dialog_new(parent,
                                    _("Erase History?"),
                                    _("Everything in history will be erased. You may wish to export your history before erasing!"));

    adw_message_dialog_add_responses(ADW_MESSAGE_DIALOG(dialog),
                                    "cancel",  _("_Cancel"),
                                    "delete", _("_Erase Everything"),
                                    NULL);

    adw_message_dialog_set_response_appearance(ADW_MESSAGE_DIALOG(dialog), "delete", ADW_RESPONSE_DESTRUCTIVE);

    adw_message_dialog_set_default_response(ADW_MESSAGE_DIALOG(dialog), "cancel");
    adw_message_dialog_set_close_response(ADW_MESSAGE_DIALOG(dialog), "cancel");

    g_signal_connect(ADW_MESSAGE_DIALOG(dialog), "response", G_CALLBACK(message_cb), self);

    gtk_window_present(GTK_WINDOW(dialog));
}

char* get_full_conversation_history(void) {
    const struct history_session *session;
    GString *full_history = g_string_new(NULL);
    size_t idx = 0;

    // First, determine the total number of sessions
    while (get_history_session(idx) != NULL) {
        idx++;
    }

    // Now iterate over the sessions in forward order
    for (size_t i = idx; i > 0; i--) {
        session = get_history_session(i - 1);
        for (size_t j = 0; j < session->entries_count; j++) {
            const struct history_entry *entry = &session->entries[j];
            for (size_t k = 0; k < entry->tokens_count; k++) {
                g_string_append(full_history, entry->tokens[k].token);
            }
            g_string_append_c(full_history, '\n');
        }
    }

    return g_string_free(full_history, FALSE); // FALSE means don't deallocate, return the data with a terminating null byte
}


static void send_message_cb(GtkButton *button, gpointer userdata) {
    LiveCaptionsHistoryWindow *self = LIVECAPTIONS_HISTORY_WINDOW(userdata);

    GtkLabel *response_label = GTK_LABEL(self->ai_response_label);
    gtk_label_set_text(GTK_LABEL(self->ai_response_label), "Processing...");

    OpenAI_Config config;
    config.api_url = g_settings_get_string(self->settings, "openai-url");
    config.api_key = g_settings_get_string(self->settings, "openai-key");

    char *session_text = get_full_conversation_history();
    char *system_text = g_strdup_printf("The user will ask questions relative the converation history: %s", session_text);

    OpenAI_Message messages[] = {
        {"system", system_text},
        {"user", gtk_editable_get_text(GTK_EDITABLE(self->chat_input_entry))},
    };

    OpenAI_Response response;
    if (openai_chat(&config, "gpt-4o-mini", messages, sizeof(messages) / sizeof(messages[0]), 0.7, &response)) {
        gtk_label_set_text(GTK_LABEL(self->ai_response_label), response.message_content);
    }

    free_openai_response(&response);

    gtk_editable_set_text(GTK_EDITABLE(self->chat_input_entry), "");
}

// This function will be called every second
static gboolean refresh_history_callback(gpointer userdata) {
    LiveCaptionsHistoryWindow *self = LIVECAPTIONS_HISTORY_WINDOW(userdata);

    // Check if auto-refresh is enabled
    if (g_settings_get_boolean(self->settings, "auto-refresh")) {
        refresh_cb(self);
    }

    // Returning TRUE so the function gets called again
    return G_SOURCE_CONTINUE;
}

// Callback function for when the checkbox is toggled
static void auto_refresh_toggled_cb(GtkCheckButton *button, gpointer userdata) {
    LiveCaptionsHistoryWindow *self = LIVECAPTIONS_HISTORY_WINDOW(userdata);
    gboolean active = gtk_check_button_get_active(button);
    g_settings_set_boolean(self->settings, "auto-refresh", active);
}

static void livecaptions_history_window_class_init(LiveCaptionsHistoryWindowClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-history-window.ui");

    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsHistoryWindow, scroll);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsHistoryWindow, main_box);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsHistoryWindow, ai_response_label);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsHistoryWindow, chat_input_entry);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsHistoryWindow, auto_refresh_checkbox);

    gtk_widget_class_bind_template_callback(widget_class, load_more_cb);
    gtk_widget_class_bind_template_callback(widget_class, export_cb);
    gtk_widget_class_bind_template_callback(widget_class, warn_deletion_cb);
    gtk_widget_class_bind_template_callback(widget_class, refresh_cb);
    gtk_widget_class_bind_template_callback(widget_class, send_message_cb);
    gtk_widget_class_bind_template_callback(widget_class, auto_refresh_toggled_cb);
}

// TODO: ctrl+f search
static void livecaptions_history_window_init(LiveCaptionsHistoryWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    self->settings = g_settings_new("net.sapples.LiveCaptions");

    self->session_load = 0;
    load_to(self, ++self->session_load);

    g_idle_add(force_bottom, self);
    g_idle_add(deferred_update_keep_above, self);

    // Bind the checkbox state to GSettings key
    GtkCheckButton *check_button = GTK_CHECK_BUTTON(self->auto_refresh_checkbox);
    gboolean active = g_settings_get_boolean(self->settings, "auto-refresh");
    gtk_check_button_set_active(check_button, active);

    // Add a timeout to call refresh_history_callback every second
    g_timeout_add_seconds(1, refresh_history_callback, self);
}