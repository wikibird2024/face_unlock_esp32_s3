import 'dart:async';
import 'dart:convert';

import 'package:face_unlock/core/board/board_address.dart';
import 'package:face_unlock/features/live/live_notifier.dart';
import 'package:fake_async/fake_async.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:shared_preferences/shared_preferences.dart';

const _jpeg = [0xFF, 0xD8, 7, 7, 0xFF, 0xD9];

Future<ProviderContainer> _container({
  required String address,
  required MockClientStreamHandler board,
}) async {
  SharedPreferences.setMockInitialValues({'board_address': address});
  final preferences = await SharedPreferences.getInstance();
  final container = ProviderContainer(
    overrides: [
      sharedPreferencesProvider.overrideWithValue(preferences),
      httpClientFactoryProvider.overrideWithValue(
        () => MockClient.streaming(board),
      ),
    ],
  );
  // liveProvider is autoDispose: keep it alive for the test.
  container.listen(liveProvider, (_, _) {});
  return container;
}

Future<http.StreamedResponse> _neverCalled(
  http.BaseRequest request,
  http.ByteStream body,
) => throw StateError('no request expected');

void main() {
  test('no address: no request, noAddress state', () async {
    final container = await _container(address: '', board: _neverCalled);
    addTearDown(container.dispose);

    await container.read(liveProvider.notifier).start();

    expect(container.read(liveProvider).status, LiveStatus.noAddress);
  });

  test('frames from /stream make the state live', () async {
    final body = StreamController<List<int>>();
    late Uri requested;
    final container = await _container(
      address: '127.0.0.1:8080',
      board: (request, _) async {
        requested = request.url;
        return http.StreamedResponse(
          body.stream,
          200,
          headers: {
            'content-type': 'multipart/x-mixed-replace;boundary=frameboundary',
          },
        );
      },
    );
    addTearDown(container.dispose);

    await container.read(liveProvider.notifier).start();
    body.add(
      ascii.encode(
        '\r\n--frameboundary\r\nContent-Type: image/jpeg\r\n'
        'Content-Length: ${_jpeg.length}\r\n\r\n',
      ),
    );
    body.add(_jpeg);
    await pumpEventQueue();

    expect(requested.toString(), 'http://127.0.0.1:8080/stream');
    final live = container.read(liveProvider);
    expect(live.status, LiveStatus.live);
    expect(live.frame, _jpeg);
  });

  test('an HTTP error means offline, with a retry', () async {
    final container = await _container(
      address: '10.0.0.5',
      board: (_, _) async =>
          http.StreamedResponse(Stream.value(utf8.encode('error')), 500),
    );
    addTearDown(container.dispose);

    await container.read(liveProvider.notifier).start();

    final live = container.read(liveProvider);
    expect(live.status, LiveStatus.offline);
    expect(live.message, 'Board answered HTTP 500');
    expect(live.retryIn, const Duration(seconds: 1));
  });

  test('stream ending means offline', () async {
    final container = await _container(
      address: '10.0.0.5',
      board: (_, _) async =>
          http.StreamedResponse(const Stream.empty(), 200, headers: {}),
    );
    addTearDown(container.dispose);

    await container.read(liveProvider.notifier).start();
    await pumpEventQueue();

    expect(container.read(liveProvider).status, LiveStatus.offline);
  });

  test('snapshot while stopped asks the board for /jpg', () async {
    late Uri requested;
    final container = await _container(
      address: '10.0.0.5',
      board: (request, _) async {
        requested = request.url;
        return http.StreamedResponse(Stream.value(_jpeg), 200);
      },
    );
    addTearDown(container.dispose);

    final jpeg = await container.read(liveProvider.notifier).takeSnapshot();

    expect(requested.toString(), 'http://10.0.0.5/jpg');
    expect(jpeg, _jpeg);
  });

  group('boardBaseUri', () {
    test('adds http:// and keeps the port', () {
      expect(boardBaseUri(' 192.168.1.50 ').toString(), 'http://192.168.1.50');
      expect(
        boardBaseUri('127.0.0.1:8080/x?y').toString(),
        'http://127.0.0.1:8080',
      );
    });
    test('empty is null', () => expect(boardBaseUri('  '), isNull));
  });

  test('retry wait grows 1, 2, 4 … and stops at 30 s', () {
    expect(
      [for (var i = 0; i < 8; i++) retryDelay(i).inSeconds],
      [1, 2, 4, 8, 16, 30, 30, 30],
    );
  });

  group('auto-reconnect', () {
    late int requestCount;

    Future<http.StreamedResponse> endsAtOnce(
      http.BaseRequest request,
      http.ByteStream body,
    ) async {
      requestCount++;
      return http.StreamedResponse(const Stream.empty(), 200);
    }

    setUp(() => requestCount = 0);

    test('a dropped stream reconnects after 1 s, then 2 s', () {
      fakeAsync((time) {
        late ProviderContainer container;
        _container(
          address: '10.0.0.5',
          board: endsAtOnce,
        ).then((created) => container = created);
        time.flushMicrotasks();

        container.read(liveProvider.notifier).start();
        time.flushMicrotasks();
        expect(requestCount, 1);
        expect(
          container.read(liveProvider).retryIn,
          const Duration(seconds: 1),
        );

        time.elapse(const Duration(seconds: 1));
        expect(requestCount, 2);
        expect(
          container.read(liveProvider).retryIn,
          const Duration(seconds: 2),
        );

        time.elapse(const Duration(seconds: 2));
        expect(requestCount, 3);
        container.dispose();
      });
    });

    test('Stop cancels the retry, and resume keeps it stopped', () {
      fakeAsync((time) {
        late ProviderContainer container;
        _container(
          address: '10.0.0.5',
          board: endsAtOnce,
        ).then((created) => container = created);
        time.flushMicrotasks();
        final notifier = container.read(liveProvider.notifier);

        notifier.start();
        time.flushMicrotasks();
        notifier.stop();
        notifier.resume(); // app back in front after the user pressed Stop
        time.elapse(const Duration(minutes: 1));

        expect(requestCount, 1);
        expect(container.read(liveProvider).status, LiveStatus.stopped);
        container.dispose();
      });
    });

    test('pause frees the board, resume reconnects', () {
      fakeAsync((time) {
        late ProviderContainer container;
        _container(
          address: '10.0.0.5',
          board: endsAtOnce,
        ).then((created) => container = created);
        time.flushMicrotasks();
        final notifier = container.read(liveProvider.notifier);

        notifier.start();
        time.flushMicrotasks();
        notifier.pause();
        time.elapse(const Duration(minutes: 1));
        expect(requestCount, 1, reason: 'no retries in the background');

        notifier.resume();
        time.flushMicrotasks();
        expect(requestCount, 2);
        container.dispose();
      });
    });
  });
}
