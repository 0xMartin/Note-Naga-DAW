#include <note_naga_engine/core/project_serializer.h>
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/dsp/dsp_factory.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>

NoteNagaProjectSerializer::NoteNagaProjectSerializer(NoteNagaEngine *engine)
    : m_engine(engine)
{
}

bool NoteNagaProjectSerializer::saveProject(const QString &filePath, const NoteNagaProjectMetadata &metadata)
{
    if (!m_engine) {
        m_lastError = "Engine is null";
        return false;
    }

    QJsonObject root;
    
    // Save metadata with updated modification time
    NoteNagaProjectMetadata metaCopy = metadata;
    metaCopy.modifiedAt = QDateTime::currentDateTime();
    root["metadata"] = metaCopy.toJson();
    
    // Save MIDI sequences
    QJsonArray sequencesArray;
    NoteNagaProject *project = m_engine->getProject();
    if (project) {
        for (NoteNagaMidiSeq *seq : project->getSequences()) {
            if (seq) {
                sequencesArray.append(serializeSequence(seq));
            }
        }
    }
    root["sequences"] = sequencesArray;
    
    // Save DSP blocks
    root["dspBlocks"] = serializeDSPBlocks();
    
    // Save DSP enabled state
    if (m_engine->getDSPEngine()) {
        root["dspEnabled"] = m_engine->getDSPEngine()->isDSPEnabled();
    }
    
    // Save routing table
    root["routingTable"] = serializeRoutingTable();
    
    // Write to file
    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QString("Cannot open file for writing: %1").arg(file.errorString());
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

bool NoteNagaProjectSerializer::loadProject(const QString &filePath, NoteNagaProjectMetadata &outMetadata)
{
    if (!m_engine) {
        m_lastError = "Engine is null";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QString("Cannot open file: %1").arg(file.errorString());
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull()) {
        m_lastError = QString("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Load metadata
    outMetadata = NoteNagaProjectMetadata::fromJson(root["metadata"].toObject());
    
    // Clear existing project data
    NoteNagaProject *project = m_engine->getProject();
    if (project) {
        // Clear existing sequences
        for (NoteNagaMidiSeq *seq : project->getSequences()) {
            project->removeSequence(seq);
            delete seq;
        }
    }
    
    // Load MIDI sequences
    QJsonArray sequencesArray = root["sequences"].toArray();
    for (const QJsonValue &seqVal : sequencesArray) {
        NoteNagaMidiSeq *seq = new NoteNagaMidiSeq();
        if (!deserializeSequence(seqVal.toObject(), seq)) {
            delete seq;
            return false;
        }
        project->addSequence(seq);
    }
    
    // Set first sequence as active
    if (!project->getSequences().empty()) {
        project->setActiveSequence(project->getSequences().front());
    }
    
    // Load DSP blocks
    if (!deserializeDSPBlocks(root["dspBlocks"].toArray())) {
        m_lastError = "Failed to load DSP blocks";
        // Continue anyway, DSP is not critical
    }
    
    // Load DSP enabled state
    if (m_engine->getDSPEngine() && root.contains("dspEnabled")) {
        m_engine->getDSPEngine()->setEnableDSP(root["dspEnabled"].toBool(true));
    }
    
    // Load routing table
    if (!deserializeRoutingTable(root["routingTable"].toArray())) {
        // If routing failed, create default routing
        m_engine->getMixer()->createDefaultRouting();
    }
    
    return true;
}

bool NoteNagaProjectSerializer::importMidiAsProject(const QString &midiFilePath, const NoteNagaProjectMetadata &metadata)
{
    if (!m_engine) {
        m_lastError = "Engine is null";
        return false;
    }
    
    // Load the MIDI file using existing functionality
    if (!m_engine->loadProject(midiFilePath.toStdString())) {
        m_lastError = "Failed to load MIDI file";
        return false;
    }
    
    return true;
}

bool NoteNagaProjectSerializer::createEmptyProject(const NoteNagaProjectMetadata &metadata)
{
    if (!m_engine) {
        m_lastError = "Engine is null";
        return false;
    }
    
    NoteNagaProject *project = m_engine->getProject();
    if (!project) {
        m_lastError = "Project is null";
        return false;
    }
    
    // Clear existing sequences
    for (NoteNagaMidiSeq *seq : project->getSequences()) {
        project->removeSequence(seq);
        delete seq;
    }
    
    // Create new sequence
    NoteNagaMidiSeq *seq = new NoteNagaMidiSeq(1);
    seq->setPPQ(480);
    seq->setTempo(120);
    
    // Create one default track
    NoteNagaTrack *track = seq->addTrack(0); // Piano
    if (track) {
        track->setName("Track 1");
    }
    
    project->addSequence(seq);
    project->setActiveSequence(seq);
    
    // Create default routing
    m_engine->getMixer()->createDefaultRouting();
    
    return true;
}

QJsonObject NoteNagaProjectSerializer::serializeSequence(NoteNagaMidiSeq *seq)
{
    QJsonObject obj;
    obj["id"] = seq->getId();
    obj["ppq"] = seq->getPPQ();
    obj["tempo"] = seq->getTempo();
    obj["maxTick"] = seq->getMaxTick();
    
    QJsonArray tracksArray;
    for (NoteNagaTrack *track : seq->getTracks()) {
        if (track) {
            QJsonObject trackObj;
            trackObj["id"] = track->getId();
            trackObj["name"] = QString::fromStdString(track->getName());
            trackObj["instrument"] = track->getInstrument().value_or(0);
            trackObj["channel"] = track->getChannel().value_or(0);
            
            // Serialize color
            NN_Color_t color = track->getColor();
            trackObj["color"] = QString("#%1%2%3")
                .arg(color.red, 2, 16, QChar('0'))
                .arg(color.green, 2, 16, QChar('0'))
                .arg(color.blue, 2, 16, QChar('0'));
            
            trackObj["visible"] = track->isVisible();
            trackObj["muted"] = track->isMuted();
            trackObj["solo"] = track->isSolo();
            trackObj["volume"] = static_cast<double>(track->getVolume());
            
            // Serialize notes
            trackObj["notes"] = serializeTrack(track);
            
            tracksArray.append(trackObj);
        }
    }
    obj["tracks"] = tracksArray;
    
    return obj;
}

QJsonArray NoteNagaProjectSerializer::serializeTrack(NoteNagaTrack *track)
{
    QJsonArray notesArray;
    for (const NN_Note_t &note : track->getNotes()) {
        QJsonObject noteObj;
        noteObj["id"] = static_cast<qint64>(note.id);
        noteObj["note"] = note.note;
        noteObj["start"] = note.start.value_or(0);
        noteObj["length"] = note.length.value_or(480);
        noteObj["velocity"] = note.velocity.value_or(100);
        noteObj["pan"] = note.pan.value_or(64);
        notesArray.append(noteObj);
    }
    return notesArray;
}

QJsonArray NoteNagaProjectSerializer::serializeDSPBlocks()
{
    QJsonArray blocksArray;
    
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    if (!dspEngine) return blocksArray;
    
    for (NoteNagaDSPBlockBase *block : dspEngine->getDSPBlocks()) {
        if (!block) continue;
        
        QJsonObject blockObj;
        blockObj["type"] = QString::fromStdString(block->getBlockName());
        blockObj["active"] = block->isActive();
        
        // Serialize parameters
        QJsonArray paramsArray;
        std::vector<DSPParamDescriptor> descriptors = block->getParamDescriptors();
        for (size_t i = 0; i < descriptors.size(); ++i) {
            QJsonObject paramObj;
            paramObj["name"] = QString::fromStdString(descriptors[i].name);
            paramObj["value"] = static_cast<double>(block->getParamValue(i));
            paramsArray.append(paramObj);
        }
        blockObj["parameters"] = paramsArray;
        
        blocksArray.append(blockObj);
    }
    
    return blocksArray;
}

QJsonArray NoteNagaProjectSerializer::serializeRoutingTable()
{
    QJsonArray routingArray;
    
    NoteNagaMixer *mixer = m_engine->getMixer();
    if (!mixer) return routingArray;
    
    for (const NoteNagaRoutingEntry &entry : mixer->getRoutingEntries()) {
        QJsonObject entryObj;
        entryObj["trackId"] = entry.track ? entry.track->getId() : -1;
        entryObj["output"] = QString::fromStdString(entry.output);
        entryObj["channel"] = entry.channel;
        entryObj["volume"] = static_cast<double>(entry.volume);
        entryObj["noteOffset"] = entry.note_offset;
        entryObj["pan"] = static_cast<double>(entry.pan);
        routingArray.append(entryObj);
    }
    
    return routingArray;
}

bool NoteNagaProjectSerializer::deserializeSequence(const QJsonObject &seqObj, NoteNagaMidiSeq *seq)
{
    seq->setPPQ(seqObj["ppq"].toInt(480));
    seq->setTempo(seqObj["tempo"].toInt(120));
    
    QJsonArray tracksArray = seqObj["tracks"].toArray();
    for (const QJsonValue &trackVal : tracksArray) {
        QJsonObject trackObj = trackVal.toObject();
        
        int instrument = trackObj["instrument"].toInt(0);
        NoteNagaTrack *track = seq->addTrack(instrument);
        if (!track) continue;
        
        track->setName(trackObj["name"].toString("Track").toStdString());
        track->setChannel(trackObj["channel"].toInt(0));
        track->setVisible(trackObj["visible"].toBool(true));
        track->setMuted(trackObj["muted"].toBool(false));
        track->setSolo(trackObj["solo"].toBool(false));
        track->setVolume(static_cast<float>(trackObj["volume"].toDouble(1.0)));
        
        // Parse color
        QString colorStr = trackObj["color"].toString("#5080c0");
        if (colorStr.startsWith("#") && colorStr.length() == 7) {
            int r = colorStr.mid(1, 2).toInt(nullptr, 16);
            int g = colorStr.mid(3, 2).toInt(nullptr, 16);
            int b = colorStr.mid(5, 2).toInt(nullptr, 16);
            track->setColor(NN_Color_t(r, g, b));
        }
        
        // Deserialize notes
        if (!deserializeTrack(trackObj, track)) {
            return false;
        }
    }
    
    return true;
}

bool NoteNagaProjectSerializer::deserializeTrack(const QJsonObject &trackObj, NoteNagaTrack *track)
{
    QJsonArray notesArray = trackObj["notes"].toArray();
    for (const QJsonValue &noteVal : notesArray) {
        QJsonObject noteObj = noteVal.toObject();
        
        NN_Note_t note;
        note.note = noteObj["note"].toInt();
        note.start = noteObj["start"].toInt();
        note.length = noteObj["length"].toInt(480);
        note.velocity = noteObj["velocity"].toInt(100);
        note.pan = noteObj["pan"].toInt(64);
        note.parent = track;
        
        track->addNote(note);
    }
    
    return true;
}

bool NoteNagaProjectSerializer::deserializeDSPBlocks(const QJsonArray &blocksArray)
{
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    if (!dspEngine) return false;
    
    // Clear existing DSP blocks
    for (NoteNagaDSPBlockBase *block : dspEngine->getDSPBlocks()) {
        dspEngine->removeDSPBlock(block);
        delete block;
    }
    
    // Load new blocks
    for (const QJsonValue &blockVal : blocksArray) {
        QJsonObject blockObj = blockVal.toObject();
        
        QString blockType = blockObj["type"].toString();
        NoteNagaDSPBlockBase *block = createDSPBlockByName(blockType);
        if (!block) continue;
        
        block->setActive(blockObj["active"].toBool(true));
        
        // Load parameters
        QJsonArray paramsArray = blockObj["parameters"].toArray();
        for (int i = 0; i < paramsArray.size(); ++i) {
            QJsonObject paramObj = paramsArray[i].toObject();
            float value = static_cast<float>(paramObj["value"].toDouble());
            block->setParamValue(i, value);
        }
        
        dspEngine->addDSPBlock(block);
    }
    
    return true;
}

bool NoteNagaProjectSerializer::deserializeRoutingTable(const QJsonArray &routingArray)
{
    NoteNagaMixer *mixer = m_engine->getMixer();
    NoteNagaProject *project = m_engine->getProject();
    if (!mixer || !project) return false;
    
    // Clear existing routing
    mixer->clearRoutingTable();
    
    NoteNagaMidiSeq *seq = project->getActiveSequence();
    if (!seq) return false;
    
    const std::vector<NoteNagaTrack*> &tracks = seq->getTracks();
    
    for (const QJsonValue &entryVal : routingArray) {
        QJsonObject entryObj = entryVal.toObject();
        
        int trackId = entryObj["trackId"].toInt(-1);
        if (trackId < 0 || trackId >= static_cast<int>(tracks.size())) continue;
        
        NoteNagaTrack *track = tracks[trackId];
        if (!track) continue;
        
        NoteNagaRoutingEntry entry(
            track,
            entryObj["output"].toString("any").toStdString(),
            entryObj["channel"].toInt(0),
            static_cast<float>(entryObj["volume"].toDouble(1.0)),
            entryObj["noteOffset"].toInt(0),
            static_cast<float>(entryObj["pan"].toDouble(0.0))
        );
        
        mixer->addRoutingEntry(entry);
    }
    
    return true;
}

NoteNagaDSPBlockBase *NoteNagaProjectSerializer::createDSPBlockByName(const QString &name)
{
    // Create DSP block based on type name - using factory functions
    if (name == "Gain") {
        return nn_create_audio_gain_block();
    } else if (name == "Pan") {
        return nn_create_audio_pan_block();
    } else if (name == "Single EQ" || name == "Equalizer" || name == "Single Band EQ") {
        return nn_create_single_band_eq_block();
    } else if (name == "Compressor") {
        return nn_create_compressor_block();
    } else if (name == "Multi EQ" || name == "Multi Band EQ") {
        return nn_create_multi_band_eq_block();
    } else if (name == "Limiter") {
        return nn_create_limiter_block();
    } else if (name == "Delay") {
        return nn_create_delay_block();
    } else if (name == "Reverb") {
        return nn_create_reverb_block();
    } else if (name == "Bitcrusher") {
        return nn_create_bitcrusher_block();
    } else if (name == "Tremolo") {
        return nn_create_tremolo_block();
    } else if (name == "Filter") {
        return nn_create_filter_block();
    } else if (name == "Chorus") {
        return nn_create_chorus_block();
    } else if (name == "Phaser") {
        return nn_create_phaser_block();
    } else if (name == "Flanger") {
        return nn_create_flanger_block();
    } else if (name == "Noise Gate") {
        return nn_create_noise_gate_block();
    } else if (name == "Saturator") {
        return nn_create_saturator_block();
    } else if (name == "Exciter") {
        return nn_create_exciter_block();
    } else if (name == "Stereo Imager") {
        return nn_create_stereo_imager_block();
    }
    
    return nullptr;
}
