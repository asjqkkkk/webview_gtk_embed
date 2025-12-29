import 'package:flutter_test/flutter_test.dart';
import 'package:webview_gtk_embed/webview_gtk_embed.dart';
import 'package:webview_gtk_embed/webview_gtk_embed_platform_interface.dart';
import 'package:webview_gtk_embed/webview_gtk_embed_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockWebviewGtkEmbedPlatform
    with MockPlatformInterfaceMixin
    implements WebviewGtkEmbedPlatform {

  @override
  Future<String?> getPlatformVersion() => Future.value('42');
}

void main() {
  final WebviewGtkEmbedPlatform initialPlatform = WebviewGtkEmbedPlatform.instance;

  test('$MethodChannelWebviewGtkEmbed is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelWebviewGtkEmbed>());
  });

  test('getPlatformVersion', () async {
    WebviewGtkEmbed webviewGtkEmbedPlugin = WebviewGtkEmbed();
    MockWebviewGtkEmbedPlatform fakePlatform = MockWebviewGtkEmbedPlatform();
    WebviewGtkEmbedPlatform.instance = fakePlatform;

    expect(await webviewGtkEmbedPlugin.getPlatformVersion(), '42');
  });
}
