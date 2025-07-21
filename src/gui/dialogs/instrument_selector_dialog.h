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

#include <note_naga_engine/note_naga_engine.h>

/**
 * @brief Dialog for selecting an instrument from a list of GM instruments.
 */
class InstrumentSelectorDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief Constructor for InstrumentSelectorDialog.
     * @param parent Parent widget.
     * @param gm_instruments List of GM instruments to display.
     * @param icon_provider Function to provide icons for instruments.
     * @param selected_gm_index Optional index of the initially selected instrument.
     */
    InstrumentSelectorDialog(QWidget* parent,
                            const std::vector<GMInstrument>& gm_instruments,
                            std::function<QIcon(QString)> icon_provider,
                            std::optional<int> selected_gm_index = std::nullopt);

    /**
     * @brief Get the selected GM index.
     * @return The index of the selected GM instrument, or -1 if none is selected.
     */
    int getSelectedGMIndex() const;

signals:
    /**
     * @brief Signal emitted when an instrument is selected.
     * @param gm_index The index of the selected GM instrument.
     */
    void instrumentSelected(int gm_index);

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

    void populateGroups();
    void selectGroup(const QString& icon_name, bool scroll_to_selected = false);
    void scrollToSelectedGroup(const QString& icon_name);
    void populateVariants(const QString& icon_name);
    void selectVariant(int gm_index);

    // Utility
    QMap<QString, std::vector<GMInstrument>> groupInstruments(const std::vector<GMInstrument>& gm_instruments);
    QString findGroupByGMIndex(int gm_index);
};