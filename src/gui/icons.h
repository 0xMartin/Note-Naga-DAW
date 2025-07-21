#pragma once

#include <QColor>
#include <QIcon>
#include <QMainWindow>
#include <QString>

// SVG icon string for colored square
extern const QString COLOR_SVG_ICON;

/**
 * @brief Create an SVG icon from a string formatted SVG.
 * @param svg The SVG string to use.
 * @param color The color to apply to the SVG, default is no color. Color replacement is
 * done by replacing "CURRENT_COLOR" in the SVG string.
 * @param size The size of the icon, default is 32.
 */
extern QIcon svg_str_icon(const QString &svg, const QColor &color = QColor(), int size = 32);

/**
 * @brief Get an instrument icon by its name.
 * @param instrument_name The name of the instrument, e.g. "piano", "guitar".
 * If the icon is not found, it will return a default vinyl icon.
 */
extern QIcon instrument_icon(const QString &instrument_name = "piano");
