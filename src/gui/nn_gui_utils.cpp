#include "nn_gui_utils.h"

#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

// SVG icon string for colored square
const QString COLOR_SVG_ICON =
    "<svg width=\"20\" height=\"20\" viewBox=\"0 0 20 20\">"
    "<rect x=\"3\" y=\"3\" width=\"14\" height=\"14\" rx=\"4\" ry=\"4\" "
    "fill=\"CURRENT_COLOR\" stroke=\"#eee\" stroke-width=\"1\"/>"
    "</svg>";

QIcon svg_str_icon(const QString &svg, const QColor &color, int size) {
    QString svg_colored = svg;
    if (color.isValid()) { svg_colored.replace("CURRENT_COLOR", color.name()); }
    int scale = 3;
    QPixmap pixmap(size * scale, size * scale);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QSvgRenderer renderer(svg_colored.toUtf8());
    renderer.render(&painter);
    painter.end();
    pixmap.setDevicePixelRatio(scale);
    return QIcon(pixmap);
}

QIcon instrument_icon(const QString &instrument_name) {
    // Try to load icon from Qt resources, fallback to vinyl.svg
    QString res_path =
        QString(":/instruments_icons/%1.svg").arg(instrument_name.toLower());
    if (QFile::exists(res_path)) return QIcon(res_path);

    // fallback
    QString fallback = QString(":/instruments_icons/vinyl.svg");
    if (QFile::exists(fallback)) return QIcon(fallback);

    return QIcon();
}

QPushButton *create_small_button(const QString &iconPath, const QString &tooltip,
                                 const char *objname, int size, QWidget *parent) {
    QPushButton *btn = new QPushButton(parent);
    btn->setObjectName(objname);
    if (!iconPath.isEmpty()) btn->setIcon(QIcon(iconPath));
    btn->setToolTip(tooltip);
    btn->setFlat(true);
    btn->setFixedSize(size, size);
    QString style = R"(
        QPushButton { 
            background: transparent; 
            border: none; 
            border-radius: %2; 
            min-width: %1; 
            max-width: %1; 
            min-height: %1; 
            max-height: %1; 
            padding: 0px; 
        }
        QPushButton:hover { background: #3477c0; color: #fff; }
        QPushButton:checked {background: #3477c0;border: 1px solid #79b8ff; }
    )";
    btn->setStyleSheet(style.arg(size - 2).arg(size / 5));
    return btn;
};

Separator *create_separator(int orientation, const QColor &color, QWidget *parent) {
    return new Separator(static_cast<Separator::Orientation>(orientation), color, parent);
}

QStringList nn_std_string_list_to_qstringlist(const std::vector<std::string> &list) {
    QStringList result;
    for (const auto &str : list) {
        result.append(QString::fromStdString(str));
    }
    return result;
}
