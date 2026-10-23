import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../live_notifier.dart';

class LiveScreen extends ConsumerStatefulWidget {
  const LiveScreen({super.key});

  @override
  ConsumerState<LiveScreen> createState() => _LiveScreenState();
}

class _LiveScreenState extends ConsumerState<LiveScreen> {
  late final AppLifecycleListener _lifecycleListener;

  @override
  void initState() {
    super.initState();
    // Free the board's single viewer slot while the app is in the background.
    _lifecycleListener = AppLifecycleListener(
      onHide: () => ref.read(liveProvider.notifier).stop(),
      onShow: () => ref.read(liveProvider.notifier).start(),
    );
    Future.microtask(() => ref.read(liveProvider.notifier).start());
  }

  @override
  void dispose() {
    _lifecycleListener.dispose();
    super.dispose();
  }

  Future<void> _showSnapshot() async {
    final messenger = ScaffoldMessenger.of(context);
    try {
      final jpeg = await ref.read(liveProvider.notifier).takeSnapshot();
      if (!mounted) return;
      await showDialog<void>(
        context: context,
        builder: (context) =>
            Dialog(child: InteractiveViewer(child: Image.memory(jpeg))),
      );
    } catch (error) {
      messenger.showSnackBar(
        SnackBar(content: Text('Snapshot failed: $error')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final live = ref.watch(liveProvider);
    final notifier = ref.read(liveProvider.notifier);
    final isRunning =
        live.status == LiveStatus.live || live.status == LiveStatus.connecting;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Live'),
        actions: [
          StatusDot(status: live.status),
          const SizedBox(width: 16),
        ],
      ),
      body: Column(
        children: [
          Expanded(
            child: Center(child: _VideoArea(live: live)),
          ),
          Padding(
            padding: const EdgeInsets.all(16),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                FilledButton.icon(
                  onPressed: isRunning ? notifier.stop : notifier.start,
                  icon: Icon(isRunning ? Icons.stop : Icons.play_arrow),
                  label: Text(isRunning ? 'Stop' : 'Start'),
                ),
                const SizedBox(width: 16),
                OutlinedButton.icon(
                  onPressed: live.status == LiveStatus.noAddress
                      ? null
                      : _showSnapshot,
                  icon: const Icon(Icons.photo_camera),
                  label: const Text('Snapshot'),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _VideoArea extends StatelessWidget {
  const _VideoArea({required this.live});

  final LiveState live;

  @override
  Widget build(BuildContext context) {
    if (live.status == LiveStatus.noAddress) {
      return const Text('Set the board address in Settings ⚙');
    }
    final Uint8List? frame = live.frame;
    final message = switch (live.status) {
      LiveStatus.connecting => 'Connecting…',
      LiveStatus.stopped => 'Stopped',
      LiveStatus.busy || LiveStatus.offline => live.message,
      _ => null,
    };
    return Stack(
      alignment: Alignment.center,
      children: [
        if (frame != null)
          Opacity(
            opacity: live.status == LiveStatus.live ? 1 : 0.4,
            // gaplessPlayback keeps the old frame until the new one decodes,
            // so the video does not flicker.
            child: Image.memory(frame, gaplessPlayback: true),
          ),
        if (message != null)
          Padding(
            padding: const EdgeInsets.all(16),
            child: Text(message, textAlign: TextAlign.center),
          ),
      ],
    );
  }
}

/// 🟢 live, 🟡 connecting, 🔴 busy/offline, ⚪ stopped.
class StatusDot extends StatelessWidget {
  const StatusDot({super.key, required this.status});

  final LiveStatus status;

  @override
  Widget build(BuildContext context) {
    final (color, label) = switch (status) {
      LiveStatus.live => (Colors.green, 'online'),
      LiveStatus.connecting => (Colors.amber, 'connecting'),
      LiveStatus.busy || LiveStatus.offline => (Colors.red, 'offline'),
      LiveStatus.stopped || LiveStatus.noAddress => (Colors.grey, 'stopped'),
    };
    return Row(
      children: [
        Icon(Icons.circle, size: 12, color: color),
        const SizedBox(width: 6),
        Text(label),
      ],
    );
  }
}
