import 'dart:convert';
import 'dart:typed_data';

/// Boundary the firmware uses (`PART_BOUNDARY` in video_stream.c), used when
/// the response header has none.
const defaultBoundary = 'frameboundary';

/// Reads `boundary=...` from a multipart Content-Type header.
String? boundaryFromContentType(String? contentType) {
  if (contentType == null) return null;
  final match = RegExp(r'boundary="?([^";]+)"?').firstMatch(contentType);
  return match?.group(1)?.trim();
}

/// Splits an MJPEG body (multipart/x-mixed-replace) into JPEG frames.
///
/// The board sends each frame as:
/// `\r\n--<boundary>\r\nContent-Type: image/jpeg\r\nContent-Length: N\r\n\r\n<N bytes>`
/// Parts without Content-Length end at the next boundary.
class MjpegReader {
  MjpegReader(String boundary) : _marker = ascii.encode('--$boundary');

  final Uint8List _marker;
  static final _headerEnd = ascii.encode('\r\n\r\n');
  static final _contentLength = RegExp(
    r'content-length:\s*(\d+)',
    caseSensitive: false,
  );

  Stream<Uint8List> frames(Stream<List<int>> body) async* {
    var pending = Uint8List(0);
    await for (final chunk in body) {
      pending = _join(pending, chunk);
      while (true) {
        final partStart = _indexOf(pending, _marker, 0);
        if (partStart < 0) {
          // Keep a tail in case the marker is split across chunks.
          final keep = _marker.length - 1;
          if (pending.length > keep) {
            pending = pending.sublist(pending.length - keep);
          }
          break;
        }
        final headerEnd = _indexOf(
          pending,
          _headerEnd,
          partStart + _marker.length,
        );
        if (headerEnd < 0) {
          pending = pending.sublist(partStart);
          break;
        }
        final headers = ascii.decode(
          pending.sublist(partStart + _marker.length, headerEnd),
          allowInvalid: true,
        );
        final frameStart = headerEnd + _headerEnd.length;
        final lengthMatch = _contentLength.firstMatch(headers);
        int frameEnd;
        if (lengthMatch != null) {
          frameEnd = frameStart + int.parse(lengthMatch.group(1)!);
          if (pending.length < frameEnd) {
            pending = pending.sublist(partStart);
            break;
          }
        } else {
          final nextPart = _indexOf(pending, _marker, frameStart);
          if (nextPart < 0) {
            pending = pending.sublist(partStart);
            break;
          }
          frameEnd = nextPart;
          // Drop the \r\n that belongs to the next boundary line.
          if (frameEnd - frameStart >= 2 &&
              pending[frameEnd - 2] == 13 &&
              pending[frameEnd - 1] == 10) {
            frameEnd -= 2;
          }
        }
        yield pending.sublist(frameStart, frameEnd);
        pending = pending.sublist(frameEnd);
      }
    }
  }

  static Uint8List _join(Uint8List first, List<int> second) {
    final joined = Uint8List(first.length + second.length);
    joined.setAll(0, first);
    joined.setAll(first.length, second);
    return joined;
  }

  static int _indexOf(Uint8List bytes, List<int> pattern, int from) {
    final last = bytes.length - pattern.length;
    outer:
    for (var start = from; start <= last; start++) {
      for (var i = 0; i < pattern.length; i++) {
        if (bytes[start + i] != pattern[i]) continue outer;
      }
      return start;
    }
    return -1;
  }
}
