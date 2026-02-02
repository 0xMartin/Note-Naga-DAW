#include <note_naga_engine/core/recent_projects_manager.h>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <algorithm>

RecentProjectsManager::RecentProjectsManager()
    : m_settings("NoteNaga", "NoteNaga")
{
    load();
}

void RecentProjectsManager::addRecentProject(const QString &filePath, const QString &projectName)
{
    // Remove if already exists (we'll re-add at top)
    removeRecentProject(filePath);
    
    // Create new entry
    RecentProjectEntry entry;
    entry.filePath = filePath;
    entry.projectName = projectName;
    entry.lastOpened = QDateTime::currentDateTime();
    
    // Add at beginning
    m_recentProjects.prepend(entry);
    
    // Trim to max size
    while (m_recentProjects.size() > MAX_RECENT_PROJECTS) {
        m_recentProjects.removeLast();
    }
    
    // Update last directory
    QFileInfo fi(filePath);
    m_lastDirectory = fi.absolutePath();
    
    save();
}

void RecentProjectsManager::removeRecentProject(const QString &filePath)
{
    m_recentProjects.erase(
        std::remove_if(m_recentProjects.begin(), m_recentProjects.end(),
            [&filePath](const RecentProjectEntry &entry) {
                return entry.filePath == filePath;
            }),
        m_recentProjects.end()
    );
    save();
}

QList<RecentProjectEntry> RecentProjectsManager::getRecentProjects(bool includeInvalid) const
{
    if (includeInvalid) {
        return m_recentProjects;
    }
    
    QList<RecentProjectEntry> validProjects;
    for (const RecentProjectEntry &entry : m_recentProjects) {
        if (entry.isValid()) {
            validProjects.append(entry);
        }
    }
    return validProjects;
}

void RecentProjectsManager::clearRecentProjects()
{
    m_recentProjects.clear();
    save();
}

RecentProjectEntry RecentProjectsManager::getMostRecentProject() const
{
    for (const RecentProjectEntry &entry : m_recentProjects) {
        if (entry.isValid()) {
            return entry;
        }
    }
    return RecentProjectEntry();
}

bool RecentProjectsManager::hasRecentProjects() const
{
    for (const RecentProjectEntry &entry : m_recentProjects) {
        if (entry.isValid()) {
            return true;
        }
    }
    return false;
}

QString RecentProjectsManager::getLastProjectDirectory() const
{
    if (!m_lastDirectory.isEmpty() && QDir(m_lastDirectory).exists()) {
        return m_lastDirectory;
    }
    
    // Try to get from most recent project
    for (const RecentProjectEntry &entry : m_recentProjects) {
        QFileInfo fi(entry.filePath);
        if (fi.exists()) {
            return fi.absolutePath();
        }
    }
    
    // Fall back to documents directory
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void RecentProjectsManager::setLastProjectDirectory(const QString &directory)
{
    m_lastDirectory = directory;
    save();
}

void RecentProjectsManager::load()
{
    m_recentProjects.clear();
    
    // Load recent projects
    QJsonDocument doc = QJsonDocument::fromJson(
        m_settings.value("recentProjects").toByteArray()
    );
    
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        for (const QJsonValue &val : arr) {
            RecentProjectEntry entry = RecentProjectEntry::fromJson(val.toObject());
            if (!entry.filePath.isEmpty()) {
                m_recentProjects.append(entry);
            }
        }
    }
    
    // Load last directory
    m_lastDirectory = m_settings.value("lastProjectDirectory").toString();
}

void RecentProjectsManager::save()
{
    // Save recent projects
    QJsonArray arr;
    for (const RecentProjectEntry &entry : m_recentProjects) {
        arr.append(entry.toJson());
    }
    
    QJsonDocument doc(arr);
    m_settings.setValue("recentProjects", doc.toJson(QJsonDocument::Compact));
    
    // Save last directory
    m_settings.setValue("lastProjectDirectory", m_lastDirectory);
    
    m_settings.sync();
}
