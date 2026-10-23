import 'package:flutter/material.dart';

/// Placeholder body for tabs that are planned but not built yet.
class ComingSoon extends StatelessWidget {
  const ComingSoon({
    super.key,
    required this.title,
    required this.icon,
    required this.plan,
  });

  final String title;
  final IconData icon;

  /// What will go here and in which phase (see docs/mqtt_contract.md).
  final String plan;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(title)),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(icon, size: 64, color: Theme.of(context).disabledColor),
              const SizedBox(height: 16),
              Text(
                'Coming soon',
                style: Theme.of(context).textTheme.titleLarge,
              ),
              const SizedBox(height: 8),
              Text(plan, textAlign: TextAlign.center),
            ],
          ),
        ),
      ),
    );
  }
}
