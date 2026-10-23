import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../features/approval/ui/approval_screen.dart';
import '../features/history/ui/history_screen.dart';
import '../features/live/ui/live_screen.dart';
import '../features/settings/ui/settings_screen.dart';

class _Tab {
  const _Tab(this.path, this.label, this.icon, this.screen);

  final String path;
  final String label;
  final IconData icon;
  final Widget screen;
}

const _tabs = [
  _Tab('/live', 'Live', Icons.videocam, LiveScreen()),
  _Tab('/approve', 'Approve', Icons.how_to_reg, ApprovalScreen()),
  _Tab('/history', 'History', Icons.history, HistoryScreen()),
  _Tab('/settings', 'Settings', Icons.settings, SettingsScreen()),
];

/// A plain ShellRoute (not StatefulShellRoute): leaving a tab disposes its
/// screen, so leaving Live closes the stream.
final routerProvider = Provider<GoRouter>((ref) {
  final router = GoRouter(
    initialLocation: _tabs.first.path,
    routes: [
      ShellRoute(
        builder: (context, state, child) =>
            _TabShell(location: state.uri.path, child: child),
        routes: [
          for (final tab in _tabs)
            GoRoute(
              path: tab.path,
              pageBuilder: (context, state) =>
                  NoTransitionPage(child: tab.screen),
            ),
        ],
      ),
    ],
  );
  ref.onDispose(router.dispose);
  return router;
});

class _TabShell extends StatelessWidget {
  const _TabShell({required this.location, required this.child});

  final String location;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    final selected = _tabs.indexWhere((tab) => tab.path == location);
    return Scaffold(
      body: child,
      bottomNavigationBar: NavigationBar(
        selectedIndex: selected < 0 ? 0 : selected,
        onDestinationSelected: (index) => context.go(_tabs[index].path),
        destinations: [
          for (final tab in _tabs)
            NavigationDestination(icon: Icon(tab.icon), label: tab.label),
        ],
      ),
    );
  }
}
