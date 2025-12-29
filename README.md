# webview_gtk_embed

GTK/WebKitGTK based WebView for Flutter desktop on Linux. The plugin exposes a `GtkWebView` widget and
`WebviewGtkEmbedController` that wrap a native WebKitGTK instance embedded via Flutter platform views.

## Features

- Seamless embedding of WebKitGTK inside Flutter layouts.
- JavaScript evaluation and return values.
- Bidirectional messaging via JavaScript channels (`window.<channel>.postMessage`).
- Navigation helpers (load URL, reload, go back/forward) and basic settings like background color.

## Usage

```dart
final controller = WebviewGtkEmbedController(
  initialUrl: 'https://flutter.dev',
  javascriptChannelNames: const {'FlutterChannel'},
);

GtkWebView(
  controller: controller,
  onJavascriptMessage: (channel, message) {
    debugPrint('[$channel] $message');
  },
);
```

To send a message from JavaScript:

```javascript
window.FlutterChannel.postMessage('hello from the page');
```

See the example application for a full demo showcasing navigation, inline HTML, and messaging.
