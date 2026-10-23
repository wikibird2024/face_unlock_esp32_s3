import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

/// Loaded once in main() and passed in with an override.
final sharedPreferencesProvider = Provider<SharedPreferences>(
  (ref) => throw UnimplementedError('override sharedPreferencesProvider'),
);

/// Makes a new HTTP client. Each stream gets its own client, so closing the
/// client is how a stream is stopped. Tests swap in a MockClient.
final httpClientFactoryProvider = Provider<http.Client Function()>(
  (ref) => http.Client.new,
);

/// The board's address as the user typed it, for example `192.168.1.50`
/// or `127.0.0.1:8080`. Empty until set in Settings.
final boardAddressProvider = NotifierProvider<BoardAddress, String>(
  BoardAddress.new,
);

class BoardAddress extends Notifier<String> {
  static const _storageKey = 'board_address';

  @override
  String build() =>
      ref.watch(sharedPreferencesProvider).getString(_storageKey) ?? '';

  Future<void> save(String address) async {
    final trimmed = address.trim();
    await ref.read(sharedPreferencesProvider).setString(_storageKey, trimmed);
    state = trimmed;
  }
}

/// `192.168.1.50` → `http://192.168.1.50`. Null when empty or not a URL.
Uri? boardBaseUri(String address) {
  final trimmed = address.trim();
  if (trimmed.isEmpty) return null;
  final withScheme = trimmed.contains('://') ? trimmed : 'http://$trimmed';
  final uri = Uri.tryParse(withScheme);
  if (uri == null || uri.host.isEmpty) return null;
  return Uri(
    scheme: uri.scheme,
    host: uri.host,
    port: uri.hasPort ? uri.port : null,
  );
}
