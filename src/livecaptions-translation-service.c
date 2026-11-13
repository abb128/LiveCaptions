/* livecaptions-translation-service.c
 *
 * Owns the offline translation worker and the private D-Bus API used by it.
 */

#include "livecaptions-translation-service.h"

#define TRANSLATION_OBJECT_PATH "/net/sapples/LiveCaptions/Translation"
#define TRANSLATION_INTERFACE "net.sapples.LiveCaptions.Translation"

struct _LiveCaptionsTranslationService {
    GObject parent_instance;

    GSettings *settings;
    GSubprocess *worker;
    GDBusConnection *connection;
    GDBusNodeInfo *introspection;
    guint object_id;
    gboolean disposing;
};

G_DEFINE_TYPE(LiveCaptionsTranslationService,
              livecaptions_translation_service,
              G_TYPE_OBJECT)

enum {
    SIGNAL_CAPTION_TRANSLATION,
    SIGNAL_HOVER_TRANSLATION,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

typedef struct {
    GWeakRef service;
} WorkerWaitContext;

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='net.sapples.LiveCaptions.Translation'>"
    "    <method name='SetTranslationText'>"
    "      <arg type='s' name='text' direction='in'/>"
    "    </method>"
    "    <method name='SetHoverTranslationText'>"
    "      <arg type='s' name='text' direction='in'/>"
    "    </method>"
    "    <signal name='HoverWord'>"
    "      <arg type='s' name='word'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

gboolean
livecaptions_translation_service_is_enabled(LiveCaptionsTranslationService *self)
{
    g_return_val_if_fail(LIVECAPTIONS_IS_TRANSLATION_SERVICE(self), FALSE);

    return g_settings_get_boolean(self->settings, "show-dual-subtitles") ||
           g_settings_get_boolean(self->settings, "translate-on-hover");
}

static void
stop_worker(LiveCaptionsTranslationService *self)
{
    if(self->worker == NULL)
        return;

    g_subprocess_force_exit(self->worker);
    g_clear_object(&self->worker);
}

static void
on_worker_exited(GObject *source_object,
                 GAsyncResult *result,
                 gpointer user_data)
{
    WorkerWaitContext *context = user_data;
    LiveCaptionsTranslationService *self =
        g_weak_ref_get(&context->service);
    GSubprocess *worker = G_SUBPROCESS(source_object);
    g_autoptr(GError) error = NULL;
    gboolean waited = g_subprocess_wait_finish(worker, result, &error);

    if(self != NULL) {
        if(!waited) {
            g_warning("Could not wait for the translation worker: %s",
                      error != NULL ? error->message : "unknown error");
        } else if(self->worker == worker && !g_subprocess_get_successful(worker)) {
            g_warning("The translation worker exited unexpectedly");
        }

        if(self->worker == worker)
            g_clear_object(&self->worker);
        g_object_unref(self);
    }

    g_weak_ref_clear(&context->service);
    g_free(context);
}

void
livecaptions_translation_service_update(LiveCaptionsTranslationService *self)
{
    g_return_if_fail(LIVECAPTIONS_IS_TRANSLATION_SERVICE(self));

    if(self->disposing || self->object_id == 0)
        return;

    if(!livecaptions_translation_service_is_enabled(self)) {
        stop_worker(self);
        return;
    }

    if(self->worker != NULL)
        return;

    const gchar *override_path = g_getenv("LIVECAPTIONS_TRANSLATION_HELPER");
    g_autofree gchar *path = override_path != NULL && override_path[0] != '\0'
        ? g_strdup(override_path)
        : g_find_program_in_path("live-translate-overlay");

    if(path == NULL) {
        g_warning("Translation is enabled, but live-translate-overlay is not installed");
        return;
    }

    g_autoptr(GError) error = NULL;
    self->worker = g_subprocess_new(G_SUBPROCESS_FLAGS_NONE,
                                    &error,
                                    path,
                                    "--worker",
                                    NULL);
    if(self->worker == NULL) {
        g_warning("Could not start the translation worker: %s",
                  error != NULL ? error->message : "unknown error");
        return;
    }

    WorkerWaitContext *wait_context = g_new0(WorkerWaitContext, 1);
    g_weak_ref_init(&wait_context->service, self);
    g_subprocess_wait_async(self->worker,
                            NULL,
                            on_worker_exited,
                            wait_context);
}

static void
on_settings_changed(G_GNUC_UNUSED GSettings *settings,
                    const gchar *key,
                    gpointer user_data)
{
    LiveCaptionsTranslationService *self =
        LIVECAPTIONS_TRANSLATION_SERVICE(user_data);

    if(g_str_equal(key, "show-dual-subtitles") ||
       g_str_equal(key, "translate-on-hover"))
        livecaptions_translation_service_update(self);
}

static void
on_method_call(G_GNUC_UNUSED GDBusConnection *connection,
               G_GNUC_UNUSED const gchar *sender,
               G_GNUC_UNUSED const gchar *object_path,
               G_GNUC_UNUSED const gchar *interface_name,
               const gchar *method_name,
               GVariant *parameters,
               GDBusMethodInvocation *invocation,
               gpointer user_data)
{
    LiveCaptionsTranslationService *self =
        LIVECAPTIONS_TRANSLATION_SERVICE(user_data);
    guint signal_id;
    if(g_str_equal(method_name, "SetTranslationText")) {
        signal_id = SIGNAL_CAPTION_TRANSLATION;
    } else if(g_str_equal(method_name, "SetHoverTranslationText")) {
        signal_id = SIGNAL_HOVER_TRANSLATION;
    } else {
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown translation method: %s",
                                              method_name);
        return;
    }

    const gchar *text;
    g_variant_get(parameters, "(&s)", &text);
    g_signal_emit(self, signals[signal_id], 0, text);
    g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
}

static const GDBusInterfaceVTable interface_vtable = {
    .method_call = on_method_call,
};

gboolean
livecaptions_translation_service_register_dbus(
    LiveCaptionsTranslationService *self,
    GDBusConnection *connection,
    GError **error)
{
    g_return_val_if_fail(LIVECAPTIONS_IS_TRANSLATION_SERVICE(self), FALSE);
    g_return_val_if_fail(G_IS_DBUS_CONNECTION(connection), FALSE);

    if(self->object_id != 0)
        return TRUE;

    self->introspection = g_dbus_node_info_new_for_xml(introspection_xml, error);
    if(self->introspection == NULL)
        return FALSE;

    guint object_id = g_dbus_connection_register_object(
        connection,
        TRANSLATION_OBJECT_PATH,
        self->introspection->interfaces[0],
        &interface_vtable,
        self,
        NULL,
        error);
    if(object_id == 0) {
        g_clear_pointer(&self->introspection, g_dbus_node_info_unref);
        return FALSE;
    }

    self->connection = g_object_ref(connection);
    self->object_id = object_id;
    livecaptions_translation_service_update(self);
    return TRUE;
}

void
livecaptions_translation_service_unregister_dbus(
    LiveCaptionsTranslationService *self,
    GDBusConnection *connection)
{
    g_return_if_fail(LIVECAPTIONS_IS_TRANSLATION_SERVICE(self));

    stop_worker(self);

    GDBusConnection *registered_connection =
        connection != NULL ? connection : self->connection;
    if(self->object_id != 0 && registered_connection != NULL) {
        g_dbus_connection_unregister_object(registered_connection, self->object_id);
        self->object_id = 0;
    }

    g_clear_object(&self->connection);
    g_clear_pointer(&self->introspection, g_dbus_node_info_unref);
}

void
livecaptions_translation_service_request_hover_word(
    LiveCaptionsTranslationService *self,
    const gchar *word)
{
    g_return_if_fail(LIVECAPTIONS_IS_TRANSLATION_SERVICE(self));

    if(self->connection == NULL || self->object_id == 0)
        return;

    g_autoptr(GError) error = NULL;
    if(!g_dbus_connection_emit_signal(self->connection,
                                      NULL,
                                      TRANSLATION_OBJECT_PATH,
                                      TRANSLATION_INTERFACE,
                                      "HoverWord",
                                      g_variant_new("(s)", word != NULL ? word : ""),
                                      &error)) {
        g_warning("Could not send the hovered word to the translation worker: %s",
                  error != NULL ? error->message : "unknown error");
    }
}

static void
livecaptions_translation_service_dispose(GObject *object)
{
    LiveCaptionsTranslationService *self =
        LIVECAPTIONS_TRANSLATION_SERVICE(object);

    self->disposing = TRUE;
    if(self->settings != NULL)
        g_signal_handlers_disconnect_by_data(self->settings, self);
    livecaptions_translation_service_unregister_dbus(self, self->connection);
    g_clear_object(&self->settings);

    G_OBJECT_CLASS(livecaptions_translation_service_parent_class)->dispose(object);
}

static void
livecaptions_translation_service_class_init(LiveCaptionsTranslationServiceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = livecaptions_translation_service_dispose;

    signals[SIGNAL_CAPTION_TRANSLATION] = g_signal_new(
        "caption-translation",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        1,
        G_TYPE_STRING);
    signals[SIGNAL_HOVER_TRANSLATION] = g_signal_new(
        "hover-translation",
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

static void
livecaptions_translation_service_init(G_GNUC_UNUSED LiveCaptionsTranslationService *self)
{
}

LiveCaptionsTranslationService *
livecaptions_translation_service_new(GSettings *settings)
{
    g_return_val_if_fail(G_IS_SETTINGS(settings), NULL);

    LiveCaptionsTranslationService *self =
        g_object_new(LIVECAPTIONS_TYPE_TRANSLATION_SERVICE, NULL);
    self->settings = g_object_ref(settings);
    g_signal_connect(self->settings, "changed", G_CALLBACK(on_settings_changed), self);
    return self;
}
