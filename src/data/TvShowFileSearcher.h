#pragma once

#include "data/TvShowEpisode.h"
#include "globals/Globals.h"

#include <QDir>
#include <QObject>

/**
 * @brief The TvShowFileSearcher class
 */
class TvShowFileSearcher : public QObject
{
    Q_OBJECT
public:
    explicit TvShowFileSearcher(QObject* parent = nullptr);
    void setTvShowDirectories(QVector<SettingsDir> directories);
    static SeasonNumber getSeasonNumber(QStringList files);
    static QVector<EpisodeNumber> getEpisodeNumbers(QStringList files);
    static TvShowEpisode* loadEpisodeData(TvShowEpisode* episode);
    static TvShowEpisode* reloadEpisodeData(TvShowEpisode* episode);

public slots:
    void reload(bool force);
    void reloadEpisodes(QString showDir);
    void abort();

signals:
    void searchStarted(QString, int);
    void progress(int, int, int);
    void tvShowsLoaded(int);
    void currentDir(QString);

private:
    QVector<SettingsDir> m_directories;
    int m_progressMessageId;
    void getTvShows(QString path, QMap<QString, QVector<QStringList>>& contents);
    void scanTvShowDir(QString startPath, QString path, QVector<QStringList>& contents);
    QStringList getFiles(QString path);
    bool m_aborted;
};
