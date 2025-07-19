#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>
#include <QSizePolicy>
#include <QFrame>
#include <QSpacerItem>
#include <QGridLayout>
#include <QFont>
#include <QString>
#include <QIcon>
#include <QMap>
#include <vector>
#include <optional>
#include <functional>

#include "../../note_naga_engine/core/types.h"

class InstrumentSelectorDialog : public QDialog
{
    Q_OBJECT
public:
    InstrumentSelectorDialog(QWidget* parent,
                            const std::vector<GMInstrument>& gm_instruments,
                            std::function<QIcon(QString)> icon_provider,
                            std::optional<int> selected_gm_index = std::nullopt);

    int get_selected_gm_index() const;

signals:
    void instrument_selected(int gm_index);

private:
    std::vector<GMInstrument> gm_instruments;
    std::function<QIcon(QString)> icon_provider;
    std::optional<int> selected_gm_index;
    QString selected_group;

    QMap<QString, std::vector<GMInstrument>> groups;
    QMap<QString, int> icon_to_group_index;

    // UI widgets
    QGridLayout* group_grid;
    QVBoxLayout* variant_vbox;
    QScrollArea* group_scroll;
    QScrollArea* variant_scroll;
    QLabel* variant_title;

    void populate_groups();
    void select_group(const QString& icon_name, bool scroll_to_selected = false);
    void scroll_to_selected_group(const QString& icon_name);
    void populate_variants(const QString& icon_name);
    void select_variant(int gm_index);

    // Utility
    QMap<QString, std::vector<GMInstrument>> group_instruments(const std::vector<GMInstrument>& gm_instruments);
    QString find_group_by_gm_index(int gm_index);
};