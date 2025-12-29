import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

import 'webview_gtk_embed_platform_interface.dart';

const String _viewType = 'webview_gtk_embed/webview';

/// Exposes helper methods that still use the legacy method channel API.
class WebviewGtkEmbed {
  Future<String?> getPlatformVersion() {
    return WebviewGtkEmbedPlatform.instance.getPlatformVersion();
  }
}

typedef JavascriptMessageCallback = void Function(
    String channel, String message);

/// Widget that renders the Gtk/WebKit based WebView on Linux via a platform view.
class GtkWebView extends StatefulWidget {
  const GtkWebView({
    super.key,
    required this.controller,
    this.onJavascriptMessage,
    this.onWebViewCreated,
    this.gestureRecognizers,
  });

  final WebviewGtkEmbedController controller;
  final JavascriptMessageCallback? onJavascriptMessage;
  final ValueChanged<WebviewGtkEmbedController>? onWebViewCreated;
  final Set<Factory<OneSequenceGestureRecognizer>>? gestureRecognizers;

  @override
  State<GtkWebView> createState() => _GtkWebViewState();
}

class _GtkWebViewState extends State<GtkWebView> {
  MethodChannel? _methodChannel;

  @override
  void didUpdateWidget(covariant GtkWebView oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.controller != oldWidget.controller) {
      oldWidget.controller._detachFromWidget();
      _methodChannel?.setMethodCallHandler(null);
    }
  }

  @override
  void dispose() {
    _methodChannel?.setMethodCallHandler(null);
    _methodChannel = null;
    widget.controller._detachFromWidget();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return PlatformViewLink(
      viewType: _viewType,
      surfaceFactory: (context, controller) {
        return PlatformViewSurface(
          controller: controller,
          hitTestBehavior: PlatformViewHitTestBehavior.opaque,
          gestureRecognizers: widget.gestureRecognizers ??
              const <Factory<OneSequenceGestureRecognizer>>{},
        );
      },
      onCreatePlatformView: (params) {
        final controller = PlatformViewsService.initSurfaceAndroidView(
          id: params.id,
          viewType: _viewType,
          layoutDirection: TextDirection.ltr,
          creationParams: <String, Object?>{
            ...widget.controller._creationParams,
          },
          creationParamsCodec: const StandardMessageCodec(),
          onFocus: () => params.onFocusChanged(true),
        );
        controller.addOnPlatformViewCreatedListener((id) {
          params.onPlatformViewCreated(id);
          _onPlatformViewCreated(id);
        });
        controller.create();
        return controller;
      },
    );
  }

  Future<void> _onPlatformViewCreated(int id) async {
    _methodChannel?.setMethodCallHandler(null);
    final methodChannel = MethodChannel('webview_gtk_embed/view_$id');
    _methodChannel = methodChannel;
    widget.controller._attachToView(id, methodChannel);
    methodChannel.setMethodCallHandler((call) async {
      if (call.method == 'javascriptChannelMessage') {
        final raw = call.arguments;
        if (raw is Map<Object?, Object?>) {
          final channel = raw['channel'] as String?;
          final message = raw['message'] as String? ?? '';
          if (channel != null) {
            widget.onJavascriptMessage?.call(channel, message);
          }
        }
      }
    });
    widget.onWebViewCreated?.call(widget.controller);
    await widget.controller._ensureChannelConfiguration();
  }
}

/// Controller exposed to the embedding to allow imperative interaction with the native view.
class WebviewGtkEmbedController {
  WebviewGtkEmbedController({
    String? initialUrl,
    Set<String> javascriptChannelNames = const <String>{},
    Color? backgroundColor,
  })  : _initialUrl = initialUrl,
        _javascriptChannels = Set<String>.from(javascriptChannelNames),
        _initialBackgroundColor = backgroundColor;

  final String? _initialUrl;
  final Color? _initialBackgroundColor;
  Set<String> _javascriptChannels;
  int? _viewId;
  MethodChannel? _channel;
  bool _disposed = false;

  int get viewId {
    final id = _viewId;
    if (id == null) {
      throw StateError('WebView is not attached to the widget tree yet.');
    }
    return id;
  }

  bool get isAttached => _channel != null && !_disposed;

  Map<String, Object?> get _creationParams => <String, Object?>{
        if (_initialUrl != null) 'initialUrl': _initialUrl,
        'javascriptChannels': _javascriptChannels.toList(),
        if (_initialBackgroundColor != null)
          'backgroundColor': _initialBackgroundColor.value,
      };

  Future<void> loadUrl(String url) async {
    await _invoke('loadUrl', <String, Object?>{'url': url});
  }

  Future<void> reload() => _invoke('reload');

  Future<void> goBack() => _invoke('goBack');

  Future<void> goForward() => _invoke('goForward');

  Future<bool> canGoBack() async => await _invoke<bool>('canGoBack') ?? false;

  Future<bool> canGoForward() async =>
      await _invoke<bool>('canGoForward') ?? false;

  Future<String?> runJavascriptReturningString(String script) {
    return _invoke<String>(
        'runJavascriptReturningString', <String, Object?>{'script': script});
  }

  Future<void> runJavascript(String script) =>
      _invoke('runJavascript', <String, Object?>{'script': script});

  Future<void> updateSettings({bool? transparentBackground}) {
    final args = <String, Object?>{};
    if (transparentBackground != null) {
      args['transparentBackground'] = transparentBackground;
    }
    return _invoke('updateSettings', args);
  }

  Future<void> setJavascriptChannels(Set<String> channels) async {
    _javascriptChannels = Set<String>.from(channels);
    await _ensureChannelConfiguration();
  }

  Future<T?> _invoke<T>(String method,
      [Map<String, Object?>? arguments]) async {
    if (_disposed) {
      throw StateError('The WebviewGtkEmbedController has been disposed.');
    }
    final channel = _channel;
    if (channel == null) {
      throw StateError('WebView is not attached to the widget tree yet.');
    }
    final result = await channel.invokeMethod<T>(method, arguments);
    return result;
  }

  Future<void> dispose() async {
    if (_disposed) {
      return;
    }
    final channel = _channel;
    if (channel != null) {
      await channel.invokeMethod<void>('dispose');
      channel.setMethodCallHandler(null);
    }
    _channel = null;
    _viewId = null;
    _disposed = true;
  }

  void _attachToView(int id, MethodChannel channel) {
    if (_disposed) {
      throw StateError('Cannot attach a disposed WebviewGtkEmbedController.');
    }
    _viewId = id;
    _channel = channel;
  }

  void _detachFromWidget() {
    _channel = null;
    _viewId = null;
  }

  Future<void> _ensureChannelConfiguration() async {
    if (!isAttached) {
      return;
    }
    await _invoke('updateJavascriptChannels',
        <String, Object?>{'channels': _javascriptChannels.toList()});
  }
}
