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

    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsHistoryWindow, label);
}

static void livecaptions_history_window_init(LiveCaptionsHistoryWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));

    const struct history_session *session = get_history_session(0);

    char text[16384];
    char *head = &text[0];
    char *end = &text[16383];

    for(size_t i=0; i<session->entries_count; i++){
        if(head >= end) break;

        head += sprintf(head, "\n");
        
        const struct history_entry *entry = &session->entries[i];

        for(size_t j=0; j<entry->tokens_count; j++) {
            if(head >= end) break;
            head += sprintf(head, "%s", entry->tokens[j].token);
        }
    }

    gtk_label_set_label(self->label, text);
}

