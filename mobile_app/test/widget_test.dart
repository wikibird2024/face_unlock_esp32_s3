import 'package:face_unlock/app/app.dart';
import 'package:face_unlock/core/board/board_address.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  testWidgets('four tabs; Live asks for an address; Settings has the field', (
    tester,
  ) async {
    SharedPreferences.setMockInitialValues({});
    final preferences = await SharedPreferences.getInstance();
    await tester.pumpWidget(
      ProviderScope(
        overrides: [sharedPreferencesProvider.overrideWithValue(preferences)],
        child: const FaceUnlockApp(),
      ),
    );
    await tester.pumpAndSettle();

    final tabBar = find.byType(NavigationBar);
    for (final label in ['Live', 'Approve', 'History', 'Settings']) {
      expect(
        find.descendant(of: tabBar, matching: find.text(label)),
        findsOneWidget,
      );
    }
    expect(find.text('Set the board address in Settings ⚙'), findsOneWidget);

    await tester.tap(find.text('Settings'));
    await tester.pumpAndSettle();
    expect(find.widgetWithText(TextField, 'Board address'), findsOneWidget);

    await tester.tap(find.text('Approve'));
    await tester.pumpAndSettle();
    expect(find.text('Coming soon'), findsOneWidget);
  });
}
