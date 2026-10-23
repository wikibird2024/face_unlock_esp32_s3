import 'package:flutter/material.dart';

import '../../../app/coming_soon.dart';

class HistoryScreen extends StatelessWidget {
  const HistoryScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return const ComingSoon(
      title: 'History',
      icon: Icons.history,
      plan:
          'Who opened the door and when, from the board\'s "event" topic.\n'
          'Phase B (MQTT), see docs/mqtt_contract.md.',
    );
  }
}
