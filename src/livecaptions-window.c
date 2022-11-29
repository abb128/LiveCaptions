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
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, header_bar);
    gtk_widget_class_bind_template_child(widget_class, LiveCaptionsWindow, label);
}

static void livecaptions_window_init (LiveCaptionsWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
}
