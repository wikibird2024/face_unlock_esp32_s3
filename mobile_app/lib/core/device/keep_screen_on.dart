import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

/// Keeps the phone screen from turning off (true) or lets it (false).
/// A provider so tests can swap in a no-op instead of the plugin.
final keepScreenOnProvider = Provider<Future<void> Function(bool)>(
  (ref) => (keepOn) async {
    try {
      await WakelockPlus.toggle(enable: keepOn);
    } catch (_) {
      // Not supported on this platform: watching still works.
    }
  },
);
