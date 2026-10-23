import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/board/board_address.dart';

class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  late final TextEditingController _addressField = TextEditingController(
    text: ref.read(boardAddressProvider),
  );
  bool _isTesting = false;

  @override
  void dispose() {
    _addressField.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    await ref.read(boardAddressProvider.notifier).save(_addressField.text);
    if (!mounted) return;
    ScaffoldMessenger.of(context)
        .showSnackBar(const SnackBar(content: Text('Saved')));
  }

  /// Asks the board for its viewer page `/`.
  Future<void> _testConnection() async {
    final messenger = ScaffoldMessenger.of(context);
    final baseUri = boardBaseUri(_addressField.text);
    if (baseUri == null) {
      messenger.showSnackBar(
        const SnackBar(content: Text('Not a valid address')),
      );
      return;
    }
    setState(() => _isTesting = true);
    final client = ref.read(httpClientFactoryProvider)();
    String result;
    try {
      final response = await client
          .get(baseUri)
          .timeout(const Duration(seconds: 3));
      result = response.statusCode == 200
          ? '🟢 Board answered'
          : '🔴 Board answered HTTP ${response.statusCode}';
    } on TimeoutException {
      result = '🔴 No answer in 3 s';
    } catch (error) {
      result = '🔴 Cannot reach the board: $error';
    } finally {
      client.close();
    }
    if (!mounted) return;
    setState(() => _isTesting = false);
    messenger.showSnackBar(SnackBar(content: Text(result)));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Settings')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          TextField(
            controller: _addressField,
            decoration: const InputDecoration(
              labelText: 'Board address',
              hintText: '192.168.1.50 or 127.0.0.1:8080',
              helperText: 'IP from the board log or the console "status"',
              border: OutlineInputBorder(),
            ),
            keyboardType: TextInputType.url,
            autocorrect: false,
            onSubmitted: (_) => _save(),
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              FilledButton(onPressed: _save, child: const Text('Save')),
              const SizedBox(width: 16),
              OutlinedButton(
                onPressed: _isTesting ? null : _testConnection,
                child: Text(_isTesting ? 'Testing…' : 'Test connection'),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
