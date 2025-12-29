#ifndef FLUTTER_PLUGIN_WEBVIEW_GTK_EMBED_PLUGIN_H_
#define FLUTTER_PLUGIN_WEBVIEW_GTK_EMBED_PLUGIN_H_

#include <flutter_linux/flutter_linux.h>
#include <flutter_linux/fl_platform_view.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(WebviewGtkEmbedPlugin,
                     webview_gtk_embed_plugin,
                     WEBVIEW,
                     GTK_EMBED_PLUGIN,
                     GObject)

void webview_gtk_embed_plugin_register_with_registrar(FlPluginRegistrar* registrar);

G_END_DECLS

#endif  // FLUTTER_PLUGIN_WEBVIEW_GTK_EMBED_PLUGIN_H_
