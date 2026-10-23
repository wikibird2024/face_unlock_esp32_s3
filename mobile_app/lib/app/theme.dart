import 'package:flutter/material.dart';

const _seedColor = Colors.teal;

final lightTheme = ThemeData(colorSchemeSeed: _seedColor);

final darkTheme = ThemeData(
  colorSchemeSeed: _seedColor,
  brightness: Brightness.dark,
);
