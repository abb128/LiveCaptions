/* livecaptions-translation-settings.c
 *
 * Translation preferences, language catalog parsing, and package install UI.
 */

#include <glib/gi18n.h>

#include "livecaptions-translation-settings.h"

typedef struct {
    gchar *code;
    gchar *name;
    gboolean installed;
} TranslationLanguage;

typedef enum {
    TRANSLATION_COMMAND_LIST,
    TRANSLATION_COMMAND_INSTALL,
} TranslationCommand;

struct _LiveCaptionsTranslationSettings {
    GObject *owner;
    GSettings *settings;

    AdwSwitchRow *dual_subtitles_switch;
    AdwSwitchRow *translate_on_hover_switch;
    AdwComboRow *target_language_row;
    GtkButton *language_action_button;
    GtkButton *language_refresh_button;
    GtkLabel *status_label;
    GtkScale *text_opacity_scale;
    GtkAdjustment *settle_adjustment;
    GtkAdjustment *minimum_words_adjustment;
    GtkAdjustment *text_opacity_adjustment;

    GtkStringList *language_model;
    GPtrArray *languages;
};

typedef struct {
    LiveCaptionsTranslationSettings *settings;
    GObject *owner;
    TranslationCommand command;
    gchar *language_code;
} TranslationJob;

static void translation_language_free(gpointer data)
{
    TranslationLanguage *language = data;
    g_free(language->code);
    g_free(language->name);
    g_free(language);
}

static void translation_job_free(TranslationJob *job)
{
    g_clear_object(&job->owner);
    g_clear_pointer(&job->language_code, g_free);
    g_free(job);
}

static void set_status(LiveCaptionsTranslationSettings *self,
                       const gchar *message)
{
    gtk_label_set_text(self->status_label, message != NULL ? message : "");
}

static TranslationLanguage *get_selected_language(
    LiveCaptionsTranslationSettings *self)
{
    guint selected = adw_combo_row_get_selected(self->target_language_row);
    if(selected == GTK_INVALID_LIST_POSITION || selected >= self->languages->len)
        return NULL;

    return g_ptr_array_index(self->languages, selected);
}

static void update_language_action(LiveCaptionsTranslationSettings *self,
                                   gboolean busy)
{
    TranslationLanguage *language = get_selected_language(self);
    gtk_widget_set_sensitive(GTK_WIDGET(self->target_language_row), !busy);
    gtk_widget_set_sensitive(GTK_WIDGET(self->language_refresh_button), !busy);

    if(language == NULL) {
        gtk_button_set_label(self->language_action_button, _("Download and Use"));
        gtk_widget_set_sensitive(GTK_WIDGET(self->language_action_button), FALSE);
        return;
    }

    gtk_button_set_label(self->language_action_button,
                         language->installed ? _("Use Language") : _("Download and Use"));
    gtk_widget_set_sensitive(GTK_WIDGET(self->language_action_button), !busy);
}

static gchar *format_text_opacity(G_GNUC_UNUSED GtkScale *scale,
                                  gdouble value,
                                  G_GNUC_UNUSED gpointer user_data)
{
    return g_strdup_printf("%.0f%%", value * 100.0);
}

static void on_translation_preference_changed(GSettings *settings,
                                             G_GNUC_UNUSED const gchar *key,
                                             G_GNUC_UNUSED gpointer user_data)
{
    if(!g_settings_get_boolean(settings, "translation-settings-migrated"))
        g_settings_set_boolean(settings, "translation-settings-migrated", TRUE);
}

static void start_language_command(LiveCaptionsTranslationSettings *self,
                                   TranslationCommand command,
                                   const gchar *language_code);

static void finish_language_catalog(LiveCaptionsTranslationSettings *self,
                                    GSubprocess *process,
                                    const gchar *stdout_text)
{
    g_ptr_array_set_size(self->languages, 0);
    guint item_count = g_list_model_get_n_items(G_LIST_MODEL(self->language_model));
    if(item_count > 0)
        gtk_string_list_splice(self->language_model, 0, item_count, NULL);

    gboolean catalog_error = FALSE;
    g_autofree gchar *catalog_message = NULL;
    g_autofree gchar *preferred_language =
        g_settings_get_string(self->settings, "translation-target-language");
    guint selected = GTK_INVALID_LIST_POSITION;
    g_auto(GStrv) lines = g_strsplit(stdout_text != NULL ? stdout_text : "", "\n", -1);

    for(guint i = 0; lines[i] != NULL; i++) {
        if(g_str_has_prefix(lines[i], "ERROR\t")) {
            catalog_error = TRUE;
            g_free(catalog_message);
            catalog_message = g_strdup(lines[i] + strlen("ERROR\t"));
            continue;
        }
        if(!g_str_has_prefix(lines[i], "LANG\t"))
            continue;

        g_auto(GStrv) fields = g_strsplit(lines[i], "\t", 4);
        if(g_strv_length(fields) != 4)
            continue;

        TranslationLanguage *language = g_new0(TranslationLanguage, 1);
        language->code = g_strdup(fields[1]);
        language->name = g_strdup(fields[2]);
        language->installed = g_str_equal(fields[3], "1");

        guint index = self->languages->len;
        g_ptr_array_add(self->languages, language);
        g_autofree gchar *label = g_strdup_printf(
            language->installed ? _("%s (%s) · Installed") : _("%s (%s)"),
            language->name,
            language->code);
        gtk_string_list_append(self->language_model, label);

        if(g_str_equal(language->code, preferred_language))
            selected = index;
    }

    if(self->languages->len == 0) {
        adw_combo_row_set_selected(self->target_language_row,
                                   GTK_INVALID_LIST_POSITION);
        if(catalog_error && catalog_message != NULL)
            set_status(self, catalog_message);
        else if(!g_subprocess_get_successful(process))
            set_status(self, _("Could not load the language catalog."));
        else
            set_status(self, _("No English translation packages were found."));
    } else {
        if(selected == GTK_INVALID_LIST_POSITION)
            selected = 0;
        adw_combo_row_set_selected(self->target_language_row, selected);

        if(catalog_error && catalog_message != NULL) {
            g_autofree gchar *message = g_strdup_printf(
                _("Showing installed languages; catalog refresh failed: %s"),
                catalog_message);
            set_status(self, message);
        } else {
            set_status(self,
                       _("Choose a language, then download it once to use it offline."));
        }
    }

    update_language_action(self, FALSE);
}

static void finish_language_install(LiveCaptionsTranslationSettings *self,
                                    GSubprocess *process,
                                    const gchar *language_code,
                                    const gchar *stdout_text,
                                    const gchar *stderr_text)
{
    if(g_subprocess_get_successful(process) &&
       stdout_text != NULL &&
       g_str_has_prefix(stdout_text, "OK\t")) {
        if(language_code != NULL) {
            g_settings_set_string(self->settings,
                                  "translation-target-language",
                                  language_code);
            gint revision = g_settings_get_int(self->settings,
                                               "translation-packages-revision");
            g_settings_set_int(self->settings,
                               "translation-packages-revision",
                               revision == G_MAXINT ? 0 : revision + 1);
        }

        set_status(self,
                   _("Language package installed. Translation is ready to use offline."));
        livecaptions_translation_settings_refresh(self);
        return;
    }

    const gchar *detail = stdout_text;
    if(detail == NULL || detail[0] == '\0')
        detail = stderr_text;
    if(detail == NULL || detail[0] == '\0')
        detail = _("Check the app runtime and network access.");
    if(g_str_has_prefix(detail, "ERROR\t"))
        detail += strlen("ERROR\t");

    g_autofree gchar *message = g_strdup_printf(_("Language download failed: %s"), detail);
    set_status(self, message);
    update_language_action(self, FALSE);
}

static void on_language_command_finished(GObject *source_object,
                                         GAsyncResult *result,
                                         gpointer user_data)
{
    GSubprocess *process = G_SUBPROCESS(source_object);
    TranslationJob *job = user_data;
    g_autofree gchar *stdout_text = NULL;
    g_autofree gchar *stderr_text = NULL;
    g_autoptr(GError) error = NULL;

    if(!g_subprocess_communicate_utf8_finish(process,
                                             result,
                                             &stdout_text,
                                             &stderr_text,
                                             &error)) {
        g_autofree gchar *message = g_strdup_printf(
            _("Translation command failed: %s"),
            error != NULL ? error->message : _("unknown error"));
        set_status(job->settings, message);
        update_language_action(job->settings, FALSE);
        translation_job_free(job);
        return;
    }

    if(job->command == TRANSLATION_COMMAND_INSTALL)
        finish_language_install(job->settings,
                                process,
                                job->language_code,
                                stdout_text,
                                stderr_text);
    else
        finish_language_catalog(job->settings, process, stdout_text);

    translation_job_free(job);
}

static void start_language_command(LiveCaptionsTranslationSettings *self,
                                   TranslationCommand command,
                                   const gchar *language_code)
{
    update_language_action(self, TRUE);

    const gchar *override_path = g_getenv("LIVECAPTIONS_TRANSLATION_HELPER");
    g_autofree gchar *path = override_path != NULL && override_path[0] != '\0'
        ? g_strdup(override_path)
        : g_find_program_in_path("live-translate-overlay");
    if(path == NULL) {
        set_status(self, _("The translation helper is not installed in this app."));
        update_language_action(self, FALSE);
        return;
    }

    g_autoptr(GError) error = NULL;
    GSubprocess *process;
    if(command == TRANSLATION_COMMAND_INSTALL) {
        process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                   G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                   &error,
                                   path,
                                   "--install-language",
                                   language_code,
                                   NULL);
    } else {
        process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                   G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                   &error,
                                   path,
                                   "--list-languages",
                                   NULL);
    }

    if(process == NULL) {
        g_autofree gchar *message = g_strdup_printf(
            _("Could not start translation helper: %s"),
            error != NULL ? error->message : _("unknown error"));
        set_status(self, message);
        update_language_action(self, FALSE);
        return;
    }

    TranslationJob *job = g_new0(TranslationJob, 1);
    job->settings = self;
    job->owner = g_object_ref(self->owner);
    job->command = command;
    job->language_code = g_strdup(language_code);
    g_subprocess_communicate_utf8_async(process,
                                        NULL,
                                        NULL,
                                        on_language_command_finished,
                                        job);
    g_object_unref(process);
}

void
livecaptions_translation_settings_refresh(LiveCaptionsTranslationSettings *self)
{
    g_return_if_fail(self != NULL);
    set_status(self, _("Loading available languages…"));
    start_language_command(self, TRANSLATION_COMMAND_LIST, NULL);
}

static void on_target_language_selected(G_GNUC_UNUSED GObject *object,
                                        G_GNUC_UNUSED GParamSpec *pspec,
                                        gpointer user_data)
{
    update_language_action(user_data, FALSE);
}

static void on_language_action(GtkButton *button G_GNUC_UNUSED,
                               gpointer user_data)
{
    LiveCaptionsTranslationSettings *self = user_data;
    TranslationLanguage *language = get_selected_language(self);
    if(language == NULL)
        return;

    if(language->installed) {
        g_settings_set_string(self->settings,
                              "translation-target-language",
                              language->code);
        g_autofree gchar *message = g_strdup_printf(
            _("Using English → %s. Translation works offline."), language->name);
        set_status(self, message);
        return;
    }

    g_autofree gchar *message = g_strdup_printf(
        _("Downloading English → %s…"), language->name);
    set_status(self, message);
    start_language_command(self, TRANSLATION_COMMAND_INSTALL, language->code);
}

static void on_refresh_clicked(GtkButton *button G_GNUC_UNUSED,
                               gpointer user_data)
{
    livecaptions_translation_settings_refresh(user_data);
}

LiveCaptionsTranslationSettings *
livecaptions_translation_settings_new(GObject *owner,
                                      GSettings *settings,
                                      AdwSwitchRow *dual_subtitles_switch,
                                      AdwSwitchRow *translate_on_hover_switch,
                                      AdwComboRow *target_language_row,
                                      GtkButton *language_action_button,
                                      GtkButton *language_refresh_button,
                                      GtkLabel *status_label,
                                      GtkAdjustment *settle_adjustment,
                                      GtkAdjustment *minimum_words_adjustment,
                                      GtkScale *text_opacity_scale,
                                      GtkAdjustment *text_opacity_adjustment)
{
    g_return_val_if_fail(G_IS_OBJECT(owner), NULL);
    g_return_val_if_fail(G_IS_SETTINGS(settings), NULL);
    g_return_val_if_fail(ADW_IS_COMBO_ROW(target_language_row), NULL);

    LiveCaptionsTranslationSettings *self = g_new0(
        LiveCaptionsTranslationSettings, 1);
    self->owner = owner;
    self->settings = g_object_ref(settings);
    self->dual_subtitles_switch = dual_subtitles_switch;
    self->translate_on_hover_switch = translate_on_hover_switch;
    self->target_language_row = target_language_row;
    self->language_action_button = language_action_button;
    self->language_refresh_button = language_refresh_button;
    self->status_label = status_label;
    self->text_opacity_scale = text_opacity_scale;
    self->settle_adjustment = settle_adjustment;
    self->minimum_words_adjustment = minimum_words_adjustment;
    self->text_opacity_adjustment = text_opacity_adjustment;
    self->language_model = gtk_string_list_new(NULL);
    self->languages = g_ptr_array_new_with_free_func(translation_language_free);

    adw_combo_row_set_model(target_language_row, G_LIST_MODEL(self->language_model));
    g_settings_bind(settings,
                    "show-dual-subtitles",
                    dual_subtitles_switch,
                    "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings,
                    "translate-on-hover",
                    translate_on_hover_switch,
                    "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings,
                    "translation-settle-ms",
                    settle_adjustment,
                    "value",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings,
                    "translation-min-words",
                    minimum_words_adjustment,
                    "value",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings,
                    "translation-text-opacity",
                    text_opacity_adjustment,
                    "value",
                    G_SETTINGS_BIND_DEFAULT);

    g_signal_connect(text_opacity_scale,
                     "format-value",
                     G_CALLBACK(format_text_opacity),
                     self);
    g_signal_connect(target_language_row,
                     "notify::selected",
                     G_CALLBACK(on_target_language_selected),
                     self);
    g_signal_connect(language_action_button,
                     "clicked",
                     G_CALLBACK(on_language_action),
                     self);
    g_signal_connect(language_refresh_button,
                     "clicked",
                     G_CALLBACK(on_refresh_clicked),
                     self);
    g_signal_connect(settings,
                     "changed::translation-target-language",
                     G_CALLBACK(on_translation_preference_changed),
                     self);
    g_signal_connect(settings,
                     "changed::translation-settle-ms",
                     G_CALLBACK(on_translation_preference_changed),
                     self);
    g_signal_connect(settings,
                     "changed::translation-min-words",
                     G_CALLBACK(on_translation_preference_changed),
                     self);

    gtk_scale_add_mark(GTK_SCALE(text_opacity_scale), 0.5, GTK_POS_TOP, NULL);
    update_language_action(self, FALSE);
    livecaptions_translation_settings_refresh(self);
    return self;
}

void
livecaptions_translation_settings_free(LiveCaptionsTranslationSettings *self)
{
    if(self == NULL)
        return;

    g_signal_handlers_disconnect_by_data(self->target_language_row, self);
    g_signal_handlers_disconnect_by_data(self->language_action_button, self);
    g_signal_handlers_disconnect_by_data(self->language_refresh_button, self);
    g_signal_handlers_disconnect_by_data(self->text_opacity_scale, self);
    g_signal_handlers_disconnect_by_data(self->settings, self);

    g_settings_unbind(G_OBJECT(self->dual_subtitles_switch), "active");
    g_settings_unbind(G_OBJECT(self->translate_on_hover_switch), "active");
    g_settings_unbind(G_OBJECT(self->settle_adjustment), "value");
    g_settings_unbind(G_OBJECT(self->minimum_words_adjustment), "value");
    g_settings_unbind(G_OBJECT(self->text_opacity_adjustment), "value");
    g_clear_object(&self->settings);
    g_clear_object(&self->language_model);
    g_clear_pointer(&self->languages, g_ptr_array_unref);
    g_free(self);
}
