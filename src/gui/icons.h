#pragma once

#include <QMainWindow>
#include <QString>
#include <QColor>
#include <QIcon>

// SVG icon string for colored square
extern const QString COLOR_SVG_ICON;

// Create a QIcon from SVG string, optionally with color and size
QIcon svg_str_icon(const QString& svg, const QColor& color = QColor(), int size = 32);

// Load instrument icon from Qt resources, fallback to vinyl.svg
QIcon instrument_icon(const QString& instrument_name = "piano");
