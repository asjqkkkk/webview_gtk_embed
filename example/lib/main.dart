import 'package:flutter/material.dart';
import 'package:webview_gtk_embed/webview_gtk_embed.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  final WebviewGtkEmbedController _controller = WebviewGtkEmbedController(
    initialUrl: 'https://flutter.dev',
    javascriptChannelNames: const {'FlutterChannel'},
  );
  String _lastMessage = 'No message yet';
  String _status = 'Idle';
  bool _webViewReady = false;

  Future<void> _loadFlutterSite() async {
    if (!_webViewReady) return;
    await _controller.loadUrl('https://flutter.dev');
    setState(() => _status = 'Loading flutter.dev...');
  }

  Future<void> _loadInlinePage() async {
    if (!_webViewReady) return;
    const script = r"""
document.body.innerHTML = '<h1>Inline WebView</h1><button id="send">Send message</button>';
document.getElementById('send').addEventListener('click', function() {
  window.FlutterChannel.postMessage('Hi from WebKit');
});
""";
    await _controller.runJavascript(
        "document.open();document.write('<html><body></body></html>');document.close();");
    await _controller.runJavascript(script);
    setState(() => _status = 'Custom HTML injected');
  }

  Future<void> _sendMessageToPage() async {
    if (!_webViewReady) return;
    await _controller.runJavascript(
        "window.FlutterChannel.postMessage('Ping from Flutter ${DateTime.now().toIso8601String()}')");
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Webview GTK Embed demo'),
        ),
        body: Column(
          children: <Widget>[
            Padding(
              padding: const EdgeInsets.all(8),
              child: Text('Status: $_status'),
            ),
            Expanded(
              child: GtkWebView(
                controller: _controller,
                onWebViewCreated: (controller) {
                  setState(() {
                    _webViewReady = true;
                    _status = 'WebView ready';
                  });
                },
                onJavascriptMessage: (channel, message) {
                  setState(() {
                    _lastMessage = '[$channel] $message';
                  });
                },
              ),
            ),
            Padding(
              padding: const EdgeInsets.all(8),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: <Widget>[
                  Text('Last JavaScript message: $_lastMessage'),
                  const SizedBox(height: 12),
                  Wrap(
                    spacing: 8,
                    children: <Widget>[
                      ElevatedButton(
                        onPressed: _loadFlutterSite,
                        child: const Text('Load flutter.dev'),
                      ),
                      ElevatedButton(
                        onPressed: _loadInlinePage,
                        child: const Text('Load inline HTML'),
                      ),
                      ElevatedButton(
                        onPressed: _sendMessageToPage,
                        child: const Text('Ping JS'),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
