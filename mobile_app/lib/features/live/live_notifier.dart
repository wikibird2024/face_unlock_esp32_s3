import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:http/http.dart' as http;

import '../../core/board/board_address.dart';
import '../../core/board/mjpeg_reader.dart';

enum LiveStatus { stopped, noAddress, connecting, live, busy, offline }

class LiveState {
  const LiveState(this.status, {this.frame, this.message});

  final LiveStatus status;

  /// Last JPEG received. Kept when the stream drops, so the screen can
  /// still show (and snapshot) the last picture.
  final Uint8List? frame;
  final String? message;
}

const connectTimeout = Duration(seconds: 5);

/// Longer than this without a frame counts as offline.
const frameTimeout = Duration(seconds: 5);

/// Live view state. Disposed when the Live tab is left, which closes the
/// stream: the board serves one viewer at a time.
final liveProvider = NotifierProvider.autoDispose<LiveNotifier, LiveState>(
  LiveNotifier.new,
);

class LiveNotifier extends Notifier<LiveState> {
  http.Client? _client;
  StreamSubscription<Uint8List>? _frameSubscription;
  Timer? _frameTimer;

  @override
  LiveState build() {
    ref.onDispose(_close);
    return const LiveState(LiveStatus.stopped);
  }

  Future<void> start() async {
    _close();
    final baseUri = boardBaseUri(ref.read(boardAddressProvider));
    if (baseUri == null) {
      state = const LiveState(LiveStatus.noAddress);
      return;
    }
    state = LiveState(LiveStatus.connecting, frame: state.frame);
    final client = ref.read(httpClientFactoryProvider)();
    _client = client;
    try {
      final response = await client
          .send(http.Request('GET', baseUri.replace(path: '/stream')))
          .timeout(connectTimeout);
      if (_client != client) return; // stopped while connecting
      if (response.statusCode == 503) {
        _close();
        state = LiveState(
          LiveStatus.busy,
          frame: state.frame,
          message: 'Someone else is watching',
        );
        return;
      }
      if (response.statusCode != 200) {
        _fail('Board answered HTTP ${response.statusCode}');
        return;
      }
      final boundary =
          boundaryFromContentType(response.headers['content-type']) ??
          defaultBoundary;
      _frameSubscription = MjpegReader(boundary)
          .frames(response.stream)
          .listen(
            (frame) {
              state = LiveState(LiveStatus.live, frame: frame);
              _restartFrameTimer();
            },
            onError: (Object error) => _fail('Stream error: $error'),
            onDone: () => _fail('Board closed the stream'),
          );
      _restartFrameTimer();
    } on TimeoutException {
      // The board's HTTP server is one task: while another viewer streams,
      // it does not answer at all instead of sending 503.
      if (_client == client) _fail('No answer (offline, or someone watching)');
    } catch (error) {
      if (_client == client) _fail('Cannot reach the board: $error');
    }
  }

  void stop() {
    _close();
    state = LiveState(LiveStatus.stopped, frame: state.frame);
  }

  /// While live, the board's single HTTP task is busy with the stream, so the
  /// snapshot is the current frame. Otherwise ask the board for `/jpg`.
  Future<Uint8List> takeSnapshot() async {
    final frame = state.frame;
    if (state.status == LiveStatus.live && frame != null) return frame;
    final baseUri = boardBaseUri(ref.read(boardAddressProvider));
    if (baseUri == null) {
      throw StateError('Set the board address in Settings');
    }
    final client = ref.read(httpClientFactoryProvider)();
    try {
      final response = await client
          .get(baseUri.replace(path: '/jpg'))
          .timeout(connectTimeout);
      if (response.statusCode != 200) {
        throw StateError('Board answered HTTP ${response.statusCode}');
      }
      return response.bodyBytes;
    } finally {
      client.close();
    }
  }

  void _restartFrameTimer() {
    _frameTimer?.cancel();
    _frameTimer = Timer(frameTimeout, () => _fail('No frames for 5 s'));
  }

  void _fail(String message) {
    _close();
    state = LiveState(LiveStatus.offline, frame: state.frame, message: message);
  }

  void _close() {
    _frameTimer?.cancel();
    _frameTimer = null;
    _frameSubscription?.cancel();
    _frameSubscription = null;
    _client?.close();
    _client = null;
  }
}
