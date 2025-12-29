import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'webview_gtk_embed_method_channel.dart';

abstract class WebviewGtkEmbedPlatform extends PlatformInterface {
  /// Constructs a WebviewGtkEmbedPlatform.
  WebviewGtkEmbedPlatform() : super(token: _token);

  static final Object _token = Object();

  static WebviewGtkEmbedPlatform _instance = MethodChannelWebviewGtkEmbed();

  /// The default instance of [WebviewGtkEmbedPlatform] to use.
  ///
  /// Defaults to [MethodChannelWebviewGtkEmbed].
  static WebviewGtkEmbedPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [WebviewGtkEmbedPlatform] when
  /// they register themselves.
  static set instance(WebviewGtkEmbedPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }
}
