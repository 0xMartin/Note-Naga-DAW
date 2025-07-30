#pragma once

#include "components/separator.h"
#include <QColor>
#include <QFrame>
#include <QIcon>
#include <QPushButton>
#include <QString>
#include <QStringList>

// SVG icon string for colored square
extern const QString COLOR_SVG_ICON;

/**
 * @brief Create an SVG icon from a string formatted SVG.
 * @param svg The SVG string to use.
 * @param color The color to apply to the SVG, default is no color. Color replacement is
 * done by replacing "CURRENT_COLOR" in the SVG string.
 * @param size The size of the icon, default is 32.
 */
extern QIcon svg_str_icon(const QString &svg, const QColor &color = QColor(),
                          int size = 32);

/**
 * @brief Get an instrument icon by its name.
 * @param instrument_name The name of the instrument, e.g. "piano", "guitar".
 * If the icon is not found, it will return a default vinyl icon.
 */
extern QIcon instrument_icon(const QString &instrument_name = "piano");

/**
 * Creates a small button with an icon and tooltip. Used for UI elements like
 * small action buttons in widgets.
 * @param iconPath Path to the icon file.
 * @param tooltip Tooltip text for the button.
 * @param objname Object name for the button.
 * @param size Size of the button, default is 32x32 pixels.
 * @param parent Parent widget for the button, default is nullptr.
 * @return Pointer to the created QPushButton.
 */
extern QPushButton *create_small_button(const QString &iconPath, const QString &tooltip,
                                        const char *objname, int size = 24,
                                        QWidget *parent = nullptr);

/**
 * @brief Create a separator widget with a specified orientation.
 * @param orientation The orientation of the separator, default is Vertical.
 * @param color The color of the separator, default is "#19191f".
 * @param parent The parent widget for the separator, default is nullptr.
 * @return Pointer to the created Separator.
 */
extern Separator *create_separator(int orientation = Separator::Vertical,
                                   const QColor &color = QColor("#19191f"),
                                   QWidget *parent = nullptr);

/**
 * @brief Convert a vector of std::string to QStringList.
 * @param list The vector of std::string to convert.
 * @return QStringList containing the converted strings.
 */                                  
extern QStringList nn_std_string_list_to_qstringlist(const std::vector<std::string> &list);