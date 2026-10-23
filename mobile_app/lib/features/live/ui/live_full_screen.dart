import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../live_notifier.dart';
import 'live_video.dart';

/// Full-screen, landscape video with pinch zoom. Opened on top of the Live
/// tab, so the same stream keeps running underneath.
class LiveFullScreen extends ConsumerStatefulWidget {
  const LiveFullScreen({super.key});

  @override
  ConsumerState<LiveFullScreen> createState() => _LiveFullScreenState();
}

class _LiveFullScreenState extends ConsumerState<LiveFullScreen> {
  @override
  void initState() {
    super.initState();
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);
  }

  @override
  void dispose() {
    SystemChrome.setEnabledSystemUIMode(SystemUiMode.edgeToEdge);
    SystemChrome.setPreferredOrientations([]); // back to any orientation
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final live = ref.watch(liveProvider);
    return Scaffold(
      backgroundColor: Colors.black,
      body: Stack(
        children: [
          Positioned.fill(
            child: InteractiveViewer(
              maxScale: 5,
              child: Center(child: LiveVideo(live: live)),
            ),
          ),
          SafeArea(
            child: Padding(
              padding: const EdgeInsets.all(8),
              child: Row(
                children: [
                  IconButton.filledTonal(
                    tooltip: 'Exit full screen',
                    onPressed: () => Navigator.of(context).pop(),
                    icon: const Icon(Icons.fullscreen_exit),
                  ),
                  const SizedBox(width: 12),
                  DefaultTextStyle(
                    style: const TextStyle(color: Colors.white),
                    child: StatusDot(status: live.status),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
