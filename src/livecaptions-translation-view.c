/* livecaptions-translation-view.c
 *
 * Owns pointer tracking and the word translation popover used by the
 * captions window.
 */

#include "livecaptions-translation-view.h"

#define HOVER_LEAVE_DELAY_MS 60

struct _LiveCaptionsTranslationView {
    GObject parent_instance;

    GtkLabel *caption_label;
    GSettings *settings;
    GtkPopover *popover;
    GtkLabel *hover_label;
    GtkEventControllerMotion *caption_motion;
    GtkEventControllerMotion *popover_motion;

    gchar *hovered_word;
    gdouble pointer_x;
    gdouble pointer_y;
    gboolean pointer_valid;
    guint leave_timeout_id;
};

G_DEFINE_TYPE(LiveCaptionsTranslationView,
              livecaptions_translation_view,
              G_TYPE_OBJECT)

enum {
    SIGNAL_HOVER_WORD_CHANGED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean is_word_character(gunichar character)
{
    return g_unichar_isalnum(character) ||
           character == '\'' ||
           character == 0x2019 ||
           character == '-';
}

static gchar *word_at_point(GtkLabel *label, gdouble x, gdouble y)
{
    const gchar *text = gtk_label_get_text(label);
    if(text == NULL || text[0] == '\0')
        return NULL;

    PangoLayout *layout = gtk_label_get_layout(label);
    if(layout == NULL)
        return NULL;

    gint layout_x = 0;
    gint layout_y = 0;
    gint byte_index = 0;
    gint trailing = 0;
    gtk_label_get_layout_offsets(label, &layout_x, &layout_y);

    gboolean inside = pango_layout_xy_to_index(
        layout,
        (gint)((x - layout_x) * PANGO_SCALE),
        (gint)((y - layout_y) * PANGO_SCALE),
        &byte_index,
        &trailing);
    gsize text_length = strlen(text);
    if(!inside || byte_index < 0 || (gsize)byte_index >= text_length)
        return NULL;

    glong character_count = g_utf8_strlen(text, -1);
    glong character_index = g_utf8_pointer_to_offset(text, text + byte_index) + trailing;
    if(character_index < 0 || character_index >= character_count)
        return NULL;

    const gchar *word_start = g_utf8_offset_to_pointer(text, character_index);
    const gchar *word_end = g_utf8_next_char(word_start);
    if(!is_word_character(g_utf8_get_char(word_start)))
        return NULL;

    while(word_start > text) {
        const gchar *previous = g_utf8_find_prev_char(text, word_start);
        if(previous == NULL || !is_word_character(g_utf8_get_char(previous)))
            break;
        word_start = previous;
    }

    while(*word_end != '\0' && is_word_character(g_utf8_get_char(word_end)))
        word_end = g_utf8_next_char(word_end);

    return g_strndup(word_start, word_end - word_start);
}

static void cancel_leave_timeout(LiveCaptionsTranslationView *self)
{
    if(self->leave_timeout_id == 0)
        return;

    g_source_remove(self->leave_timeout_id);
    self->leave_timeout_id = 0;
}

static void reset_display(LiveCaptionsTranslationView *self)
{
    self->pointer_valid = FALSE;
    g_clear_pointer(&self->hovered_word, g_free);

    if(self->hover_label != NULL)
        gtk_label_set_text(self->hover_label, "");
    if(self->popover != NULL)
        gtk_popover_popdown(self->popover);
}

void
livecaptions_translation_view_clear(LiveCaptionsTranslationView *self)
{
    g_return_if_fail(LIVECAPTIONS_IS_TRANSLATION_VIEW(self));

    gboolean had_word = self->hovered_word != NULL && self->hovered_word[0] != '\0';
    cancel_leave_timeout(self);
    reset_display(self);

    if(had_word)
        g_signal_emit(self, signals[SIGNAL_HOVER_WORD_CHANGED], 0, "");
}

static gboolean finish_leave(gpointer user_data)
{
    LiveCaptionsTranslationView *self =
        LIVECAPTIONS_TRANSLATION_VIEW(user_data);
    self->leave_timeout_id = 0;

    if((self->caption_motion != NULL &&
        gtk_event_controller_motion_contains_pointer(self->caption_motion)) ||
       (self->popover_motion != NULL &&
        gtk_event_controller_motion_contains_pointer(self->popover_motion)))
        return G_SOURCE_REMOVE;

    livecaptions_translation_view_clear(self);
    return G_SOURCE_REMOVE;
}

static void schedule_leave(LiveCaptionsTranslationView *self)
{
    cancel_leave_timeout(self);
    self->leave_timeout_id = g_timeout_add_full(
        G_PRIORITY_DEFAULT,
        HOVER_LEAVE_DELAY_MS,
        finish_leave,
        g_object_ref(self),
        g_object_unref);
}

static void set_hovered_word(LiveCaptionsTranslationView *self,
                             gchar *word,
                             gdouble x,
                             gdouble y)
{
    cancel_leave_timeout(self);

    if(!g_settings_get_boolean(self->settings, "translate-on-hover")) {
        g_free(word);
        livecaptions_translation_view_clear(self);
        return;
    }

    if(word == NULL) {
        livecaptions_translation_view_clear(self);
        return;
    }
    if(g_strcmp0(word, self->hovered_word) == 0) {
        g_free(word);
        self->pointer_x = x;
        self->pointer_y = y;
        self->pointer_valid = TRUE;
        return;
    }

    reset_display(self);
    self->hovered_word = word;
    self->pointer_x = x;
    self->pointer_y = y;
    self->pointer_valid = TRUE;

    GdkRectangle pointing_to = {
        .x = (gint)x,
        .y = (gint)y,
        .width = 1,
        .height = 1,
    };
    gtk_popover_set_pointing_to(self->popover, &pointing_to);
    g_signal_emit(self, signals[SIGNAL_HOVER_WORD_CHANGED], 0, self->hovered_word);
}

static void update_hovered_word(LiveCaptionsTranslationView *self,
                                gdouble x,
                                gdouble y)
{
    set_hovered_word(self,
                     word_at_point(self->caption_label, x, y),
                     x,
                     y);
}

static void on_caption_motion(G_GNUC_UNUSED GtkEventControllerMotion *controller,
                              gdouble x,
                              gdouble y,
                              gpointer user_data)
{
    update_hovered_word(LIVECAPTIONS_TRANSLATION_VIEW(user_data), x, y);
}

static void on_caption_leave(G_GNUC_UNUSED GtkEventControllerMotion *controller,
                             gpointer user_data)
{
    schedule_leave(LIVECAPTIONS_TRANSLATION_VIEW(user_data));
}

static void on_popover_motion(G_GNUC_UNUSED GtkEventControllerMotion *controller,
                              gdouble x,
                              gdouble y,
                              gpointer user_data)
{
    LiveCaptionsTranslationView *self =
        LIVECAPTIONS_TRANSLATION_VIEW(user_data);
    cancel_leave_timeout(self);

    graphene_point_t popover_point = GRAPHENE_POINT_INIT(x, y);
    graphene_point_t caption_point;
    if(!gtk_widget_compute_point(GTK_WIDGET(self->popover),
                                 GTK_WIDGET(self->caption_label),
                                 &popover_point,
                                 &caption_point))
        return;

    g_autofree gchar *word = word_at_point(self->caption_label,
                                           caption_point.x,
                                           caption_point.y);
    if(word != NULL)
        set_hovered_word(self,
                         g_steal_pointer(&word),
                         caption_point.x,
                         caption_point.y);
}

static void on_popover_leave(G_GNUC_UNUSED GtkEventControllerMotion *controller,
                             gpointer user_data)
{
    schedule_leave(LIVECAPTIONS_TRANSLATION_VIEW(user_data));
}

static void on_caption_changed(G_GNUC_UNUSED GtkLabel *label,
                               G_GNUC_UNUSED GParamSpec *pspec,
                               gpointer user_data)
{
    LiveCaptionsTranslationView *self =
        LIVECAPTIONS_TRANSLATION_VIEW(user_data);
    if(!self->pointer_valid || self->hovered_word == NULL)
        return;

    if(!g_settings_get_boolean(self->settings, "translate-on-hover")) {
        livecaptions_translation_view_clear(self);
        return;
    }

    g_autofree gchar *word = word_at_point(self->caption_label,
                                           self->pointer_x,
                                           self->pointer_y);
    if(g_strcmp0(word, self->hovered_word) == 0)
        return;

    set_hovered_word(self,
                     g_steal_pointer(&word),
                     self->pointer_x,
                     self->pointer_y);
}

static void on_hover_setting_changed(GSettings *settings,
                                     const gchar *key,
                                     gpointer user_data)
{
    if(!g_settings_get_boolean(settings, key))
        livecaptions_translation_view_clear(
            LIVECAPTIONS_TRANSLATION_VIEW(user_data));
}

static void livecaptions_translation_view_dispose(GObject *object)
{
    LiveCaptionsTranslationView *self =
        LIVECAPTIONS_TRANSLATION_VIEW(object);

    livecaptions_translation_view_clear(self);

    if(self->settings != NULL)
        g_signal_handlers_disconnect_by_data(self->settings, self);
    if(self->caption_label != NULL) {
        g_signal_handlers_disconnect_by_data(self->caption_label, self);
        if(self->caption_motion != NULL)
            gtk_widget_remove_controller(GTK_WIDGET(self->caption_label),
                                         GTK_EVENT_CONTROLLER(self->caption_motion));
    }
    if(self->popover != NULL) {
        if(self->popover_motion != NULL)
            gtk_widget_remove_controller(GTK_WIDGET(self->popover),
                                         GTK_EVENT_CONTROLLER(self->popover_motion));
        if(gtk_widget_get_parent(GTK_WIDGET(self->popover)) != NULL)
            gtk_widget_unparent(GTK_WIDGET(self->popover));
    }

    g_clear_object(&self->caption_label);
    g_clear_object(&self->settings);
    g_clear_object(&self->hover_label);
    g_clear_object(&self->popover);

    G_OBJECT_CLASS(livecaptions_translation_view_parent_class)->dispose(object);
}

static void livecaptions_translation_view_finalize(GObject *object)
{
    LiveCaptionsTranslationView *self =
        LIVECAPTIONS_TRANSLATION_VIEW(object);
    g_clear_pointer(&self->hovered_word, g_free);
    G_OBJECT_CLASS(livecaptions_translation_view_parent_class)->finalize(object);
}

static void livecaptions_translation_view_class_init(
    LiveCaptionsTranslationViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = livecaptions_translation_view_dispose;
    object_class->finalize = livecaptions_translation_view_finalize;

    signals[SIGNAL_HOVER_WORD_CHANGED] = g_signal_new(
        "hover-word-changed",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        1,
        G_TYPE_STRING);
}

static void livecaptions_translation_view_init(
    G_GNUC_UNUSED LiveCaptionsTranslationView *self)
{
}

LiveCaptionsTranslationView *
livecaptions_translation_view_new(GtkLabel *caption_label, GSettings *settings)
{
    g_return_val_if_fail(GTK_IS_LABEL(caption_label), NULL);
    g_return_val_if_fail(G_IS_SETTINGS(settings), NULL);

    LiveCaptionsTranslationView *self =
        g_object_new(LIVECAPTIONS_TYPE_TRANSLATION_VIEW, NULL);
    self->caption_label = g_object_ref(caption_label);
    self->settings = g_object_ref(settings);

    self->popover = GTK_POPOVER(gtk_popover_new());
    g_object_ref_sink(self->popover);
    gtk_popover_set_autohide(self->popover, FALSE);

    self->hover_label = GTK_LABEL(gtk_label_new(""));
    g_object_ref_sink(self->hover_label);
    gtk_widget_set_margin_start(GTK_WIDGET(self->hover_label), 10);
    gtk_widget_set_margin_end(GTK_WIDGET(self->hover_label), 10);
    gtk_widget_set_margin_top(GTK_WIDGET(self->hover_label), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(self->hover_label), 6);
    gtk_popover_set_child(self->popover, GTK_WIDGET(self->hover_label));
    gtk_widget_set_parent(GTK_WIDGET(self->popover), GTK_WIDGET(caption_label));

    self->caption_motion = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(self->caption_motion), GTK_PHASE_CAPTURE);
    g_signal_connect_object(self->caption_motion,
                            "motion",
                            G_CALLBACK(on_caption_motion),
                            self,
                            0);
    g_signal_connect_object(self->caption_motion,
                            "leave",
                            G_CALLBACK(on_caption_leave),
                            self,
                            0);
    gtk_widget_add_controller(GTK_WIDGET(caption_label),
                              GTK_EVENT_CONTROLLER(self->caption_motion));

    self->popover_motion = GTK_EVENT_CONTROLLER_MOTION(gtk_event_controller_motion_new());
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(self->popover_motion), GTK_PHASE_CAPTURE);
    g_signal_connect_object(self->popover_motion,
                            "motion",
                            G_CALLBACK(on_popover_motion),
                            self,
                            0);
    g_signal_connect_object(self->popover_motion,
                            "leave",
                            G_CALLBACK(on_popover_leave),
                            self,
                            0);
    gtk_widget_add_controller(GTK_WIDGET(self->popover),
                              GTK_EVENT_CONTROLLER(self->popover_motion));

    g_signal_connect_object(caption_label,
                            "notify::label",
                            G_CALLBACK(on_caption_changed),
                            self,
                            0);
    g_signal_connect(settings,
                     "changed::translate-on-hover",
                     G_CALLBACK(on_hover_setting_changed),
                     self);

    return self;
}

void
livecaptions_translation_view_set_translation(LiveCaptionsTranslationView *self,
                                              const gchar *text)
{
    g_return_if_fail(LIVECAPTIONS_IS_TRANSLATION_VIEW(self));

    const gchar *translation = text != NULL ? text : "";
    gtk_label_set_text(self->hover_label, translation);
    if(translation[0] == '\0' || self->hovered_word == NULL)
        gtk_popover_popdown(self->popover);
    else
        gtk_popover_popup(self->popover);
}

void
livecaptions_translation_view_set_opacity(LiveCaptionsTranslationView *self,
                                          gdouble opacity)
{
    g_return_if_fail(LIVECAPTIONS_IS_TRANSLATION_VIEW(self));
    gtk_widget_set_opacity(GTK_WIDGET(self->hover_label), CLAMP(opacity, 0.0, 1.0));
}
