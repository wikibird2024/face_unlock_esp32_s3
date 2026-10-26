import 'dart:typed_data';

import 'package:flutter/material.dart';

import '../live_notifier.dart';

/// The picture plus a status line on top of it. Used by the Live tab and
/// the full-screen page.
class LiveVideo extends StatelessWidget {
  const LiveVideo({super.key, required this.live});

  final LiveState live;

  @override
  Widget build(BuildContext context) {
    if (live.status == LiveStatus.noAddress) {
      return const Text('Set the board address in Settings ⚙');
    }
    final Uint8List? frame = live.frame;
    final retryIn = live.retryIn;
    final message = switch (live.status) {
      LiveStatus.connecting => 'Connecting…',
      LiveStatus.stopped => 'Stopped',
      LiveStatus.offline =>
        retryIn == null
            ? live.message
            : '${live.message}\nRetrying in ${retryIn.inSeconds} s',
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
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: Colors.black54,
              borderRadius: BorderRadius.circular(8),
            ),
            child: Text(
              message,
              textAlign: TextAlign.center,
              style: const TextStyle(color: Colors.white),
            ),
          ),
        if (live.status == LiveStatus.live)
          Positioned(
            right: 8,
            bottom: 8,
            child: _Badge('${live.framesPerSecond} fps'),
          ),
      ],
    );
  }
}

class _Badge extends StatelessWidget {
  const _Badge(this.text);

  final String text;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
      decoration: BoxDecoration(
        color: Colors.black54,
        borderRadius: BorderRadius.circular(4),
      ),
      child: Text(
        text,
        style: const TextStyle(color: Colors.white, fontSize: 12),
      ),
    );
  }
}

/// 🟢 live, 🟡 connecting, 🔴 offline, ⚪ stopped.
class StatusDot extends StatelessWidget {
  const StatusDot({super.key, required this.status});

  final LiveStatus status;

  @override
  Widget build(BuildContext context) {
    final (color, label) = switch (status) {
      LiveStatus.live => (Colors.green, 'online'),
      LiveStatus.connecting => (Colors.amber, 'connecting'),
      LiveStatus.offline => (Colors.red, 'offline'),
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
