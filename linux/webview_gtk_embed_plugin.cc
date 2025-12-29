#include "include/webview_gtk_embed/webview_gtk_embed_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <flutter_linux/fl_platform_view.h>
#include <gtk/gtk.h>
#include <sys/utsname.h>
#include <webkit2/webkit2.h>


#include <cinttypes>
#include <cstring>

namespace {

constexpr char kPluginChannel[] = "webview_gtk_embed";
constexpr char kViewType[] = "webview_gtk_embed/webview";
constexpr char kJavascriptCallbackMethod[] = "javascriptChannelMessage";

}  // namespace

struct _WebviewGtkEmbedPlugin {
  GObject parent_instance;
  FlBinaryMessenger* messenger;
};

G_DEFINE_TYPE(WebviewGtkEmbedPlugin, webview_gtk_embed_plugin, G_TYPE_OBJECT)

typedef struct _WebviewGtkEmbedViewFactory {
  FlPlatformViewFactory parent_instance;
  FlBinaryMessenger* messenger;
} WebviewGtkEmbedViewFactory;

typedef struct _WebviewGtkEmbedPlatformView {
  GObject parent_instance;
  FlBinaryMessenger* messenger;
  GtkWidget* container;
  WebKitWebView* web_view;
  FlMethodChannel* view_channel;
  gint64 view_id;
  GHashTable* javascript_handlers;
} WebviewGtkEmbedPlatformView;

typedef struct {
  FlMethodCall* call;
  WebviewGtkEmbedPlatformView* view;
  gboolean expects_result;
} RunJavascriptData;

typedef struct {
  gchar* channel_name;
  FlMethodChannel* dart_channel;
} JavascriptCallbackData;

static void javascript_callback_data_free(gpointer data) {
  auto* payload = static_cast<JavascriptCallbackData*>(data);
  g_free(payload->channel_name);
  g_free(payload);
}

// Forward declarations
static void webview_gtk_embed_platform_view_iface_init(FlPlatformViewInterface* iface);
static void platform_view_method_call_cb(FlMethodChannel* channel,
                                         FlMethodCall* method_call,
                                         gpointer user_data);
static void plugin_method_call_cb(FlMethodChannel* channel,
                                  FlMethodCall* method_call,
                                  gpointer user_data);
static WebviewGtkEmbedPlatformView* webview_gtk_embed_platform_view_new(FlBinaryMessenger* messenger,
                                                                        gint64 view_id,
                                                                        FlValue* args);

G_DEFINE_TYPE_WITH_CODE(WebviewGtkEmbedPlatformView,
                        webview_gtk_embed_platform_view,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(fl_platform_view_get_type(),
                                              webview_gtk_embed_platform_view_iface_init))

G_DEFINE_TYPE(WebviewGtkEmbedViewFactory,
              webview_gtk_embed_view_factory,
              fl_platform_view_factory_get_type())

static void webview_gtk_embed_plugin_dispose(GObject* object) {
  auto* self = WEBVIEW_GTK_EMBED_PLUGIN(object);
  g_clear_object(&self->messenger);
  G_OBJECT_CLASS(webview_gtk_embed_plugin_parent_class)->dispose(object);
}

static void webview_gtk_embed_plugin_handle_method_call(WebviewGtkEmbedPlugin* self,
                                                        FlMethodCall* method_call) {
  const gchar* method = fl_method_call_get_name(method_call);
  if (strcmp(method, "getPlatformVersion") == 0) {
    struct utsname info = {};
    uname(&info);
    g_autofree gchar* version = g_strdup_printf("Linux %s", info.release);
    g_autoptr(FlValue) result = fl_value_new_string(version);
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  fl_method_call_respond(method_call, response, nullptr);
}

static void plugin_method_call_cb(FlMethodChannel* channel,
                                  FlMethodCall* method_call,
                                  gpointer user_data) {
  auto* plugin = WEBVIEW_GTK_EMBED_PLUGIN(user_data);
  webview_gtk_embed_plugin_handle_method_call(plugin, method_call);
}

static void webview_gtk_embed_plugin_class_init(WebviewGtkEmbedPluginClass* klass) {
  auto* object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = webview_gtk_embed_plugin_dispose;
}

static void webview_gtk_embed_plugin_init(WebviewGtkEmbedPlugin* self) {}

static void view_factory_dispose(GObject* object) {
  auto* factory = WEBVIEW_GTK_EMBED_VIEW_FACTORY(object);
  g_clear_object(&factory->messenger);
  G_OBJECT_CLASS(webview_gtk_embed_view_factory_parent_class)->dispose(object);
}

static FlPlatformView* view_factory_create(FlPlatformViewFactory* factory,
                                           int64_t view_id,
                                           FlValue* args) {
  auto* self = WEBVIEW_GTK_EMBED_VIEW_FACTORY(factory);
  WebviewGtkEmbedPlatformView* view = webview_gtk_embed_platform_view_new(self->messenger, view_id, args);
  return FL_PLATFORM_VIEW(view);
}

static void webview_gtk_embed_view_factory_class_init(WebviewGtkEmbedViewFactoryClass* klass) {
  auto* object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = view_factory_dispose;

  auto* factory_class = FL_PLATFORM_VIEW_FACTORY_CLASS(klass);
  factory_class->create_platform_view = view_factory_create;
}

static void webview_gtk_embed_view_factory_init(WebviewGtkEmbedViewFactory* factory) {
  g_autoptr(FlStandardMessageCodec) codec = fl_standard_message_codec_new();
  fl_platform_view_factory_set_create_args_codec(FL_PLATFORM_VIEW_FACTORY(factory), FL_MESSAGE_CODEC(codec));
}

static WebviewGtkEmbedViewFactory* webview_gtk_embed_view_factory_create(FlBinaryMessenger* messenger) {
  auto* factory = WEBVIEW_GTK_EMBED_VIEW_FACTORY(
      g_object_new(webview_gtk_embed_view_factory_get_type(), nullptr));
  factory->messenger = FL_BINARY_MESSENGER(g_object_ref(messenger));
  return factory;
}

static GtkWidget* platform_view_get_view(FlPlatformView* platform_view) {
  auto* view = WEBVIEW_GTK_EMBED_PLATFORM_VIEW(platform_view);
  return view->container;
}

static void webview_gtk_embed_platform_view_iface_init(FlPlatformViewInterface* iface) {
  iface->get_view = platform_view_get_view;
}

static void run_javascript_finished_cb(WebKitWebView* web_view,
                                       GAsyncResult* result,
                                       gpointer user_data) {
  auto* data = static_cast<RunJavascriptData*>(user_data);
  g_autoptr(GError) error = nullptr;
  WebKitJavascriptResult* js_result = webkit_web_view_run_javascript_finish(web_view, result, &error);
  FlMethodCall* call = data->call;
  gboolean expects_result = data->expects_result;
  g_autoptr(FlMethodResponse) response = nullptr;
  if (error != nullptr) {
    response = FL_METHOD_RESPONSE(
        fl_method_error_response_new("webview_gtk_embed", error->message, nullptr));
  } else {
    FlValue* value = nullptr;
    if (expects_result && js_result != nullptr) {
      g_autofree gchar* result_str = webkit_javascript_result_to_string(js_result);
      value = fl_value_new_string(result_str != nullptr ? result_str : "");
    }
    response = FL_METHOD_RESPONSE(fl_method_success_response_new(value));
  }
  fl_method_call_respond(call, response, nullptr);
  if (js_result != nullptr) {
    webkit_javascript_result_unref(js_result);
  }
  g_object_unref(call);
  g_object_unref(data->view);
  g_free(data);
}

static void clear_javascript_channels(WebviewGtkEmbedPlatformView* view) {
  if (view->javascript_handlers == nullptr) {
    return;
  }
  WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(view->web_view);
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  g_hash_table_iter_init(&iter, view->javascript_handlers);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    const gchar* name = static_cast<const gchar*>(key);
    auto* handler_id = static_cast<gulong*>(value);
    g_signal_handler_disconnect(manager, *handler_id);
    webkit_user_content_manager_unregister_script_message_handler(manager, name);
  }
  g_hash_table_remove_all(view->javascript_handlers);
  webkit_user_content_manager_remove_all_scripts(manager);
}

static void javascript_message_cb(WebKitUserContentManager* manager,
                                  WebKitJavascriptResult* result,
                                  gpointer user_data) {
  auto* data = static_cast<JavascriptCallbackData*>(user_data);
  if (data->dart_channel == nullptr) {
    return;
  }
  g_autofree gchar* payload = webkit_javascript_result_to_string(result);
  FlValue* args = fl_value_new_map();
  fl_value_set_string(args, "channel", fl_value_new_string(data->channel_name));
  fl_value_set_string(args, "message", fl_value_new_string(payload != nullptr ? payload : ""));
  fl_method_channel_invoke_method(data->dart_channel,
                                  kJavascriptCallbackMethod,
                                  args,
                                  nullptr,
                                  nullptr,
                                  nullptr);
  fl_value_unref(args);
}

static void register_javascript_channel(WebviewGtkEmbedPlatformView* view, const gchar* name) {
  if (view->javascript_handlers == nullptr) {
    view->javascript_handlers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  }
  if (g_hash_table_lookup(view->javascript_handlers, name) != nullptr) {
    return;
  }
  WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(view->web_view);
  webkit_user_content_manager_register_script_message_handler(manager, name);
  auto* data = g_new0(JavascriptCallbackData, 1);
  data->channel_name = g_strdup(name);
  data->dart_channel = view->view_channel;
  g_autofree gchar* signal_name = g_strdup_printf("script-message-received::%s", name);
  gulong handler_id = g_signal_connect_data(manager,
                                            signal_name,
                                            G_CALLBACK(javascript_message_cb),
                                            data,
                                            javascript_callback_data_free,
                                            static_cast<GConnectFlags>(0));
  auto* stored = g_new0(gulong, 1);
  *stored = handler_id;
  g_hash_table_insert(view->javascript_handlers, g_strdup(name), stored);

  g_autofree gchar* script_source = g_strdup_printf(
      "if (window.%1$s === undefined) { window.%1$s = { postMessage: function(message) { window.webkit.messageHandlers['%1$s'].postMessage(String(message)); } }; }",
      name);
  WebKitUserScript* script = webkit_user_script_new(script_source,
                                                    WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                                    WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                                                    nullptr,
                                                    nullptr);
  webkit_user_content_manager_add_user_script(manager, script);
  webkit_user_script_unref(script);
}

static void update_javascript_channels(WebviewGtkEmbedPlatformView* view, FlValue* list) {
  clear_javascript_channels(view);
  if (list == nullptr || fl_value_get_type(list) != FL_VALUE_TYPE_LIST) {
    return;
  }
  for (size_t i = 0; i < fl_value_get_length(list); ++i) {
    FlValue* value = fl_value_get_list_value(list, i);
    if (value == nullptr || fl_value_get_type(value) != FL_VALUE_TYPE_STRING) {
      continue;
    }
    register_javascript_channel(view, fl_value_get_string(value));
  }
}

static FlValue* lookup_arg(FlValue* args, const gchar* key) {
  if (args == nullptr || fl_value_get_type(args) != FL_VALUE_TYPE_MAP) {
    return nullptr;
  }
  return fl_value_lookup_string(args, key);
}

static void handle_method_call(WebviewGtkEmbedPlatformView* view, FlMethodCall* method_call) {
  const gchar* method = fl_method_call_get_name(method_call);
  FlValue* args = fl_method_call_get_args(method_call);

  if (strcmp(method, "loadUrl") == 0) {
    FlValue* url_value = lookup_arg(args, "url");
    if (url_value == nullptr || fl_value_get_type(url_value) != FL_VALUE_TYPE_STRING) {
      g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(
          fl_method_error_response_new("webview_gtk_embed", "Missing url", nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }
    webkit_web_view_load_uri(view->web_view, fl_value_get_string(url_value));
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "reload") == 0) {
    webkit_web_view_reload(view->web_view);
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "goBack") == 0) {
    if (webkit_web_view_can_go_back(view->web_view)) {
      webkit_web_view_go_back(view->web_view);
    }
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "goForward") == 0) {
    if (webkit_web_view_can_go_forward(view->web_view)) {
      webkit_web_view_go_forward(view->web_view);
    }
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "canGoBack") == 0) {
    g_autoptr(FlValue) result = fl_value_new_bool(webkit_web_view_can_go_back(view->web_view));
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "canGoForward") == 0) {
    g_autoptr(FlValue) result = fl_value_new_bool(webkit_web_view_can_go_forward(view->web_view));
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(result));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "runJavascript") == 0 || strcmp(method, "runJavascriptReturningString") == 0) {
    FlValue* script_value = lookup_arg(args, "script");
    if (script_value == nullptr || fl_value_get_type(script_value) != FL_VALUE_TYPE_STRING) {
      g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(
          fl_method_error_response_new("webview_gtk_embed", "Missing script", nullptr));
      fl_method_call_respond(method_call, response, nullptr);
      return;
    }
    auto* data = g_new0(RunJavascriptData, 1);
    data->call = FL_METHOD_CALL(g_object_ref(method_call));
    data->view = WEBVIEW_GTK_EMBED_PLATFORM_VIEW(g_object_ref(view));
    data->expects_result = strcmp(method, "runJavascriptReturningString") == 0;
    webkit_web_view_run_javascript(view->web_view,
                                   fl_value_get_string(script_value),
                                   nullptr,
                                   run_javascript_finished_cb,
                                   data);
    return;
  }

  if (strcmp(method, "updateJavascriptChannels") == 0) {
    update_javascript_channels(view, lookup_arg(args, "channels"));
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "updateSettings") == 0) {
    FlValue* transparent = lookup_arg(args, "transparentBackground");
    if (transparent != nullptr && fl_value_get_type(transparent) == FL_VALUE_TYPE_BOOL) {
      GdkRGBA rgba;
      rgba.red = 1.0;
      rgba.green = 1.0;
      rgba.blue = 1.0;
      rgba.alpha = fl_value_get_bool(transparent) ? 0.0 : 1.0;
      webkit_web_view_set_background_color(view->web_view, &rgba);
    }
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  if (strcmp(method, "dispose") == 0) {
    clear_javascript_channels(view);
    g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    fl_method_call_respond(method_call, response, nullptr);
    return;
  }

  g_autoptr(FlMethodResponse) response = FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  fl_method_call_respond(method_call, response, nullptr);
}

static void platform_view_method_call_cb(FlMethodChannel* channel,
                                         FlMethodCall* method_call,
                                         gpointer user_data) {
  auto* view = WEBVIEW_GTK_EMBED_PLATFORM_VIEW(user_data);
  handle_method_call(view, method_call);
}

static void webview_gtk_embed_platform_view_dispose(GObject* object) {
  auto* view = WEBVIEW_GTK_EMBED_PLATFORM_VIEW(object);
  clear_javascript_channels(view);
  g_clear_object(&view->view_channel);
  g_clear_object(&view->messenger);
  if (view->javascript_handlers != nullptr) {
    g_hash_table_destroy(view->javascript_handlers);
    view->javascript_handlers = nullptr;
  }
  G_OBJECT_CLASS(webview_gtk_embed_platform_view_parent_class)->dispose(object);
}

static void webview_gtk_embed_platform_view_class_init(WebviewGtkEmbedPlatformViewClass* klass) {
  auto* object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = webview_gtk_embed_platform_view_dispose;
}

static void webview_gtk_embed_platform_view_init(WebviewGtkEmbedPlatformView* view) {
  view->javascript_handlers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

static void apply_initial_params(WebviewGtkEmbedPlatformView* view, FlValue* args) {
  if (args == nullptr || fl_value_get_type(args) != FL_VALUE_TYPE_MAP) {
    return;
  }
  FlValue* initial_url = fl_value_lookup_string(args, "initialUrl");
  if (initial_url != nullptr && fl_value_get_type(initial_url) == FL_VALUE_TYPE_STRING) {
    webkit_web_view_load_uri(view->web_view, fl_value_get_string(initial_url));
  }
  FlValue* background_color = fl_value_lookup_string(args, "backgroundColor");
  if (background_color != nullptr &&
      (fl_value_get_type(background_color) == FL_VALUE_TYPE_INT ||
       fl_value_get_type(background_color) == FL_VALUE_TYPE_UINT64)) {
    guint32 value = fl_value_get_type(background_color) == FL_VALUE_TYPE_INT
                        ? static_cast<guint32>(fl_value_get_int(background_color))
                        : static_cast<guint32>(fl_value_get_uint64(background_color));
    GdkRGBA rgba;
    rgba.alpha = ((value >> 24) & 0xFF) / 255.0;
    rgba.red = ((value >> 16) & 0xFF) / 255.0;
    rgba.green = ((value >> 8) & 0xFF) / 255.0;
    rgba.blue = (value & 0xFF) / 255.0;
    webkit_web_view_set_background_color(view->web_view, &rgba);
  }
  FlValue* channels = fl_value_lookup_string(args, "javascriptChannels");
  if (channels != nullptr) {
    update_javascript_channels(view, channels);
  }
}

static WebviewGtkEmbedPlatformView* webview_gtk_embed_platform_view_new(FlBinaryMessenger* messenger,
                                                                        gint64 view_id,
                                                                        FlValue* args) {
  auto* view = WEBVIEW_GTK_EMBED_PLATFORM_VIEW(
      g_object_new(webview_gtk_embed_platform_view_get_type(), nullptr));
  view->messenger = FL_BINARY_MESSENGER(g_object_ref(messenger));
  view->view_id = view_id;

  view->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(view->container, TRUE);
  gtk_widget_set_vexpand(view->container, TRUE);

  WebKitUserContentManager* manager = webkit_user_content_manager_new();
  view->web_view = WEBKIT_WEB_VIEW(webkit_web_view_new_with_user_content_manager(manager));
  g_object_unref(manager);
  gtk_widget_set_hexpand(GTK_WIDGET(view->web_view), TRUE);
  gtk_widget_set_vexpand(GTK_WIDGET(view->web_view), TRUE);
  gtk_box_pack_start(GTK_BOX(view->container), GTK_WIDGET(view->web_view), TRUE, TRUE, 0);
  gtk_widget_show(GTK_WIDGET(view->web_view));
  gtk_widget_show(view->container);

  g_autofree gchar* channel_name = g_strdup_printf("webview_gtk_embed/view_%" G_GINT64_FORMAT, view_id);
  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  view->view_channel = fl_method_channel_new(view->messenger, channel_name, FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(view->view_channel,
                                            platform_view_method_call_cb,
                                            g_object_ref(view),
                                            g_object_unref);

  apply_initial_params(view, args);

  return view;
}

void webview_gtk_embed_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
  WebviewGtkEmbedPlugin* plugin = WEBVIEW_GTK_EMBED_PLUGIN(
      g_object_new(webview_gtk_embed_plugin_get_type(), nullptr));
  plugin->messenger = FL_BINARY_MESSENGER(g_object_ref(fl_plugin_registrar_get_messenger(registrar)));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_message_codec_new();
  g_autoptr(FlMethodChannel) channel = fl_method_channel_new(plugin->messenger,
                                                            kPluginChannel,
                                                            FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(channel,
                                            plugin_method_call_cb,
                                            g_object_ref(plugin),
                                            g_object_unref);

  WebviewGtkEmbedViewFactory* factory = webview_gtk_embed_view_factory_create(plugin->messenger);
  FlPlatformViewRegistry* registry = fl_plugin_registrar_get_platform_view_registry(registrar);
  fl_platform_view_registry_register_factory(registry,
                                             kViewType,
                                             FL_PLATFORM_VIEW_FACTORY(factory));
  g_object_unref(factory);

  g_object_unref(plugin);
}
