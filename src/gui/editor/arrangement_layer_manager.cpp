#include "arrangement_layer_manager.h"

#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/core/runtime_data.h>
#include <note_naga_engine/core/types.h>

#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>

ArrangementLayerManager::ArrangementLayerManager(NoteNagaEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine)
{
    initUI();
    refreshFromArrangement();
}

void ArrangementLayerManager::initUI()
{
    setStyleSheet(R"(
        QWidget {
            background-color: #1e1e24;
        }
        QListWidget {
            background-color: #252530;
            border: none;
            color: #cccccc;
            font-size: 12px;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #3a3a42;
        }
        QListWidget::item:selected {
            background-color: #2563eb;
            color: white;
        }
        QListWidget::item:hover:!selected {
            background-color: #2a2a35;
        }
        QPushButton {
            background-color: #3a3a42;
            color: #cccccc;
            border: none;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 11px;
        }
        QPushButton:hover {
            background-color: #454550;
        }
        QPushButton:pressed {
            background-color: #2563eb;
        }
    )");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Track list
    m_trackList = new QListWidget(this);
    m_trackList->setDragDropMode(QAbstractItemView::InternalMove);
    m_trackList->setDefaultDropAction(Qt::MoveAction);
    m_trackList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_trackList->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_trackList, 1);

    // Buttons row
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(4);

    m_addBtn = new QPushButton("+", this);
    m_addBtn->setFixedSize(32, 28);
    m_addBtn->setToolTip(tr("Add new track"));
    buttonLayout->addWidget(m_addBtn);

    m_removeBtn = new QPushButton("−", this);
    m_removeBtn->setFixedSize(32, 28);
    m_removeBtn->setToolTip(tr("Remove selected track"));
    buttonLayout->addWidget(m_removeBtn);

    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    // Connections
    connect(m_addBtn, &QPushButton::clicked, this, &ArrangementLayerManager::onAddTrack);
    connect(m_removeBtn, &QPushButton::clicked, this, &ArrangementLayerManager::onRemoveTrack);
    connect(m_trackList, &QListWidget::itemSelectionChanged, this, &ArrangementLayerManager::onTrackSelectionChanged);
    connect(m_trackList, &QListWidget::itemDoubleClicked, this, &ArrangementLayerManager::onTrackDoubleClicked);
    connect(m_trackList, &QListWidget::customContextMenuRequested, this, &ArrangementLayerManager::showContextMenu);
    
    // Handle drag/drop reordering
    connect(m_trackList->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
        // Update arrangement order based on list order
        if (!m_engine || !m_engine->getRuntimeData()) return;
        
        NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
        if (!arrangement) return;

        // Rebuild track order from list
        for (int i = 0; i < m_trackList->count(); ++i) {
            QListWidgetItem *item = m_trackList->item(i);
            int trackId = item->data(Qt::UserRole).toInt();
            // Move track to position i
            auto& tracks = arrangement->getTracks();
            for (size_t j = 0; j < tracks.size(); ++j) {
                if (tracks[j]->getId() == trackId && static_cast<int>(j) != i) {
                    arrangement->moveTrack(j, i);
                    break;
                }
            }
        }
        
        emit tracksReordered();
    });
}

void ArrangementLayerManager::refreshFromArrangement()
{
    m_trackList->clear();
    
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;

    for (auto *track : arrangement->getTracks()) {
        if (!track) continue;
        
        QListWidgetItem *item = new QListWidgetItem(m_trackList);
        item->setData(Qt::UserRole, track->getId());
        updateTrackItem(item, track);
        m_trackList->addItem(item);
    }
}

void ArrangementLayerManager::updateTrackItem(QListWidgetItem* item, NoteNagaArrangementTrack* track)
{
    if (!item || !track) return;
    
    QString displayText = QString::fromStdString(track->getName());
    
    // Add mute/solo indicators
    if (track->isMuted()) {
        displayText += " [M]";
    }
    if (track->isSolo()) {
        displayText += " [S]";
    }
    
    item->setText(displayText);
    
    // Set color indicator
    QColor color = track->getColor().toQColor();
    item->setBackground(color.darker(300));
}

NoteNagaArrangementTrack* ArrangementLayerManager::getSelectedTrack() const
{
    if (!m_engine || !m_engine->getRuntimeData()) return nullptr;
    
    QListWidgetItem *item = m_trackList->currentItem();
    if (!item) return nullptr;
    
    int trackId = item->data(Qt::UserRole).toInt();
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return nullptr;
    
    return arrangement->getTrackById(trackId);
}

void ArrangementLayerManager::onAddTrack()
{
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;

    QString name = QInputDialog::getText(this, tr("New Track"), tr("Track name:"),
                                          QLineEdit::Normal, tr("Track %1").arg(arrangement->getTrackCount() + 1));
    if (name.isEmpty()) return;

    arrangement->addTrack(name.toStdString());
    refreshFromArrangement();
}

void ArrangementLayerManager::onRemoveTrack()
{
    NoteNagaArrangementTrack *track = getSelectedTrack();
    if (!track) return;
    
    if (!m_engine || !m_engine->getRuntimeData()) return;
    
    NoteNagaArrangement *arrangement = m_engine->getRuntimeData()->getArrangement();
    if (!arrangement) return;

    int ret = QMessageBox::question(this, tr("Remove Track"),
                                     tr("Are you sure you want to remove track '%1'?")
                                         .arg(QString::fromStdString(track->getName())),
                                     QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        arrangement->removeTrack(track->getId());
        refreshFromArrangement();
    }
}

void ArrangementLayerManager::onRenameTrack()
{
    NoteNagaArrangementTrack *track = getSelectedTrack();
    if (!track) return;

    QString newName = QInputDialog::getText(this, tr("Rename Track"), tr("New name:"),
                                             QLineEdit::Normal, QString::fromStdString(track->getName()));
    if (!newName.isEmpty() && newName != QString::fromStdString(track->getName())) {
        track->setName(newName.toStdString());
        refreshFromArrangement();
        emit trackModified(track);
    }
}

void ArrangementLayerManager::onTrackSelectionChanged()
{
    NoteNagaArrangementTrack *track = getSelectedTrack();
    emit trackSelected(track);
}

void ArrangementLayerManager::onTrackDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item);
    onRenameTrack();
}

void ArrangementLayerManager::showContextMenu(const QPoint& pos)
{
    QListWidgetItem *item = m_trackList->itemAt(pos);
    
    QMenu menu(this);
    
    QAction *addAction = menu.addAction(tr("Add Track"));
    connect(addAction, &QAction::triggered, this, &ArrangementLayerManager::onAddTrack);
    
    if (item) {
        menu.addSeparator();
        
        QAction *renameAction = menu.addAction(tr("Rename"));
        connect(renameAction, &QAction::triggered, this, &ArrangementLayerManager::onRenameTrack);
        
        NoteNagaArrangementTrack *track = getSelectedTrack();
        if (track) {
            QAction *muteAction = menu.addAction(track->isMuted() ? tr("Unmute") : tr("Mute"));
            connect(muteAction, &QAction::triggered, this, [this, track]() {
                track->setMuted(!track->isMuted());
                refreshFromArrangement();
                emit trackModified(track);
            });
            
            QAction *soloAction = menu.addAction(track->isSolo() ? tr("Unsolo") : tr("Solo"));
            connect(soloAction, &QAction::triggered, this, [this, track]() {
                track->setSolo(!track->isSolo());
                refreshFromArrangement();
                emit trackModified(track);
            });
            
            QAction *colorAction = menu.addAction(tr("Change Color"));
            connect(colorAction, &QAction::triggered, this, [this, track]() {
                QColor color = QColorDialog::getColor(track->getColor().toQColor(), this, tr("Track Color"));
                if (color.isValid()) {
                    track->setColor(NN_Color_t::fromQColor(color));
                    refreshFromArrangement();
                    emit trackModified(track);
                }
            });
        }
        
        menu.addSeparator();
        
        QAction *removeAction = menu.addAction(tr("Remove"));
        connect(removeAction, &QAction::triggered, this, &ArrangementLayerManager::onRemoveTrack);
    }
    
    menu.exec(m_trackList->mapToGlobal(pos));
}
