import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'webview_gtk_embed_platform_interface.dart';

/// An implementation of [WebviewGtkEmbedPlatform] that uses method channels.
class MethodChannelWebviewGtkEmbed extends WebviewGtkEmbedPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('webview_gtk_embed');

  @override
  Future<String?> getPlatformVersion() async {
    final version =
        await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }
}
