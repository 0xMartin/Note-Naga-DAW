#pragma once

// Recent projects manager requires Qt - only available when QT_DEACTIVATED is not defined
#ifndef QT_DEACTIVATED

#include <QString>
#include <QStringList>
#include <QSettings>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Information about a recent project entry
 */
struct RecentProjectEntry {
    QString filePath;
    QString projectName;
    QDateTime lastOpened;
    
    bool isValid() const {
        return !filePath.isEmpty() && QFileInfo::exists(filePath);
    }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["filePath"] = filePath;
        obj["projectName"] = projectName;
        obj["lastOpened"] = lastOpened.toString(Qt::ISODate);
        return obj;
    }
    
    static RecentProjectEntry fromJson(const QJsonObject &obj) {
        RecentProjectEntry entry;
        entry.filePath = obj["filePath"].toString();
        entry.projectName = obj["projectName"].toString();
        entry.lastOpened = QDateTime::fromString(obj["lastOpened"].toString(), Qt::ISODate);
        return entry;
    }
};

/**
 * @brief Manager for storing and retrieving recent projects
 * 
 * Uses QSettings for cross-platform persistent storage.
 */
class RecentProjectsManager {
public:
    static constexpr int MAX_RECENT_PROJECTS = 10;
    
    RecentProjectsManager();
    ~RecentProjectsManager() = default;
    
    /**
     * @brief Add a project to the recent list
     * @param filePath Full path to the .nnproj file
     * @param projectName Name of the project (from metadata)
     */
    void addRecentProject(const QString &filePath, const QString &projectName);
    
    /**
     * @brief Remove a project from the recent list
     * @param filePath Full path to the project to remove
     */
    void removeRecentProject(const QString &filePath);
    
    /**
     * @brief Get all recent projects (sorted by last opened, newest first)
     * @param includeInvalid If false, filters out projects that no longer exist
     * @return List of recent project entries
     */
    QList<RecentProjectEntry> getRecentProjects(bool includeInvalid = false) const;
    
    /**
     * @brief Clear all recent projects
     */
    void clearRecentProjects();
    
    /**
     * @brief Get the most recently opened project
     * @return The most recent valid project, or empty entry if none exists
     */
    RecentProjectEntry getMostRecentProject() const;
    
    /**
     * @brief Check if there are any recent projects
     * @return True if at least one valid recent project exists
     */
    bool hasRecentProjects() const;
    
    /**
     * @brief Get the last opened project directory
     * @return Path to directory of most recent project, or home directory if none
     */
    QString getLastProjectDirectory() const;
    
    /**
     * @brief Set the last used directory for file dialogs
     * @param directory The directory path
     */
    void setLastProjectDirectory(const QString &directory);
    
private:
    void load();
    void save();
    
    QList<RecentProjectEntry> m_recentProjects;
    QString m_lastDirectory;
    QSettings m_settings;
};

#endif // QT_DEACTIVATED
