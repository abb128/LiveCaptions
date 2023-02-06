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

#include "livecaptions-history-window.h"

#include "history.h"
#include <april_api.h>

G_DEFINE_TYPE(LiveCaptionsHistoryWindow, livecaptions_history_window, GTK_TYPE_APPLICATION_WINDOW)

static void livecaptions_history_window_class_init(LiveCaptionsHistoryWindowClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/net/sapples/LiveCaptions/livecaptions-history-window.ui");

    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsHistoryWindow, scroll);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsHistoryWindow, label);
}

static bool force_bottom(gpointer userdata) {
    LiveCaptionsHistoryWindow *self = LIVECAPTIONS_HISTORY_WINDOW(userdata);

    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(self->scroll));
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));

    return G_SOURCE_REMOVE;
}

static void livecaptions_history_window_init(LiveCaptionsHistoryWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    self->settings = g_settings_new("net.sapples.LiveCaptions");


    PangoFontDescription *desc = pango_font_description_from_string(g_settings_get_string(self->settings, "font-name"));
    PangoAttribute *attr_font = pango_attr_font_desc_new(desc);
    PangoAttrList *attr = gtk_label_get_attributes(self->label);
    if(attr == NULL){
        attr = pango_attr_list_new();
    }
    pango_attr_list_change(attr, attr_font);
    gtk_label_set_attributes(self->label, attr);
    pango_font_description_free(desc);



    char text[16384];
    char *head = &text[0];
    char *end = &text[16383];

    const struct history_session *session = get_history_session(0); // TODO: load more than just active

    // TODO: Timestamps?
    // TODO: Respect settings

    for(size_t i=0; i<session->entries_count; i++){
        if(head >= end) break;

        head += sprintf(head, "\n");
        
        const struct history_entry *entry = &session->entries[i];

        for(size_t j=0; j<entry->tokens_count; j++) {
            if((head+16) >= end) break;

            const char *token = entry->tokens[j].token;
            if((j == 0) && (*token == ' ')) token++;

            head += sprintf(head, "%s", token);
        }
    }

    gtk_label_set_label(self->label, text);

    g_idle_add(force_bottom, self);
}

