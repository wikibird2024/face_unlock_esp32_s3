import 'package:flutter/material.dart';

import '../../../app/coming_soon.dart';

class ApprovalScreen extends StatelessWidget {
  const ApprovalScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return const ComingSoon(
      title: 'Approve',
      icon: Icons.how_to_reg,
      plan:
          'A known face at the door asks here: "alice at door  [Open] [Deny]".\n'
          'Phase B (MQTT), see docs/mqtt_contract.md.',
    );
  }
}
