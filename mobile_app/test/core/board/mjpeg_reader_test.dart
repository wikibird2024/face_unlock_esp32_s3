import 'dart:convert';
import 'dart:typed_data';

import 'package:face_unlock/core/board/mjpeg_reader.dart';
import 'package:flutter_test/flutter_test.dart';

/// Fake JPEGs: start and end markers around some bytes. The body includes
/// "\r\n\r\n" and "--" so the parser can't rely on them being absent.
final _frames = [
  Uint8List.fromList([0xFF, 0xD8, 1, 2, 3, 13, 10, 13, 10, 0xFF, 0xD9]),
  Uint8List.fromList([0xFF, 0xD8, 45, 45, 9, 9, 9, 0xFF, 0xD9]),
  Uint8List.fromList([0xFF, 0xD8, 0xFF, 0xD9]),
];

/// The exact part format of video_stream.c (with Content-Length).
Uint8List _firmwareBody(List<Uint8List> frames) {
  final body = BytesBuilder();
  for (final frame in frames) {
    body.add(
      ascii.encode(
        '\r\n--frameboundary\r\n'
        'Content-Type: image/jpeg\r\nContent-Length: ${frame.length}\r\n\r\n',
      ),
    );
    body.add(frame);
  }
  return body.toBytes();
}

/// Parts without Content-Length: each frame ends at the next boundary.
Uint8List _bodyWithoutLength(List<Uint8List> frames) {
  final body = BytesBuilder();
  for (final frame in frames) {
    body.add(
      ascii.encode('--frameboundary\r\nContent-Type: image/jpeg\r\n\r\n'),
    );
    body.add(frame);
    body.add(ascii.encode('\r\n'));
  }
  body.add(ascii.encode('--frameboundary--\r\n'));
  return body.toBytes();
}

Stream<List<int>> _inChunks(Uint8List body, int chunkSize) async* {
  for (var start = 0; start < body.length; start += chunkSize) {
    final end = (start + chunkSize).clamp(0, body.length);
    yield body.sublist(start, end);
  }
}

void main() {
  group('MjpegReader with the firmware format', () {
    for (final chunkSize in [1, 3, 7, 64, 100000]) {
      test('chunk size $chunkSize gives every frame', () async {
        final frames = await MjpegReader('frameboundary')
            .frames(_inChunks(_firmwareBody(_frames), chunkSize))
            .toList();
        expect(frames, _frames);
      });
    }
  });

  test('parts without Content-Length end at the next boundary', () async {
    final frames = await MjpegReader('frameboundary')
        .frames(_inChunks(_bodyWithoutLength(_frames), 5))
        .toList();
    expect(frames, _frames);
  });

  test('a cut-off last frame is not emitted', () async {
    final body = _firmwareBody(_frames);
    final frames = await MjpegReader('frameboundary')
        .frames(_inChunks(body.sublist(0, body.length - 2), 16))
        .toList();
    expect(frames, _frames.sublist(0, 2));
  });

  group('boundaryFromContentType', () {
    test('firmware header', () {
      expect(
        boundaryFromContentType(
          'multipart/x-mixed-replace;boundary=frameboundary',
        ),
        'frameboundary',
      );
    });
    test('quoted, with spaces', () {
      expect(
        boundaryFromContentType(
          'multipart/x-mixed-replace; boundary="abc"; charset=x',
        ),
        'abc',
      );
    });
    test('missing', () {
      expect(boundaryFromContentType('image/jpeg'), isNull);
      expect(boundaryFromContentType(null), isNull);
    });
  });
}
