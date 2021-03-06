#include "MusicFileSearcher.h"

#include <QDebug>
#include <QDirIterator>
#include <QFileInfo>
#include <QtConcurrent>

#include "../globals/Manager.h"

MusicFileSearcher::MusicFileSearcher(QObject* parent) :
    QObject(parent),
    m_progressMessageId{Constants::MusicFileSearcherProgressMessageId},
    m_aborted{false}
{
}

void MusicFileSearcher::setMusicDirectories(QVector<SettingsDir> directories)
{
    m_directories.clear();
    for (SettingsDir dir : directories) {
        QFileInfo fi(dir.path);
        if (fi.isDir()) {
            m_directories.append(dir);
        }
    }
}

void MusicFileSearcher::reload(bool force)
{
    m_aborted = false;

    emit searchStarted(tr("Searching for Music..."), m_progressMessageId);
    Manager::instance()->musicModel()->clear();

    QVector<Artist*> artists;
    QVector<Artist*> artistsFromDb;
    QVector<Album*> albums;
    QVector<Album*> albumsFromDb;

    if (force) {
        Manager::instance()->database()->clearArtists();
    }

    QMap<Artist*, QString> artistPaths;
    QMap<Album*, QString> albumPaths;
    for (SettingsDir dir : m_directories) {
        if (m_aborted) {
            break;
        }

        if (dir.autoReload) {
            Manager::instance()->database()->clearArtists(dir.path);
        }

        if (dir.autoReload || force) {
            QDirIterator it(dir.path, QDir::NoDotAndDotDot | QDir::Dirs, QDirIterator::FollowSymlinks);
            while (it.hasNext()) {
                if (m_aborted) {
                    break;
                }

                it.next();

                emit currentDir(it.fileInfo().baseName());
                Artist* artist = new Artist(it.filePath(), this);
                artist->setName(it.fileInfo().baseName());
                artists.append(artist);
                artistPaths.insert(artist, dir.path);

                QDirIterator itAlbums(it.filePath(), QDir::NoDotAndDotDot | QDir::Dirs, QDirIterator::FollowSymlinks);
                while (itAlbums.hasNext()) {
                    itAlbums.next();

                    if (itAlbums.fileInfo().baseName() == "extrafanart") {
                        continue;
                    }
                    if (itAlbums.fileInfo().baseName() == "extrathumbs") {
                        continue;
                    }

                    Album* album = new Album(itAlbums.filePath(), this);
                    album->setTitle(itAlbums.fileInfo().baseName());
                    album->setArtistObj(artist);
                    artist->addAlbum(album);
                    albums.append(album);
                    albumPaths.insert(album, dir.path);
                }
            }
        } else {
            QVector<Artist*> artistsInPath = Manager::instance()->database()->artists(dir.path);
            for (Artist* artist : artistsInPath) {
                if (artistsFromDb.count() % 20 == 0) {
                    emit currentDir(artist->path().mid(dir.path.length()));
                }
                QVector<Album*> albumsOfArtist = Manager::instance()->database()->albums(artist);
                artistsFromDb.append(artist);
                albumsFromDb.append(albumsOfArtist);
            }
        }
    }

    emit currentDir("");
    emit searchStarted(tr("Loading Music..."), m_progressMessageId);

    int current = 0;
    int max = artists.length() + albums.length() + artistsFromDb.length() + albumsFromDb.length();

    Manager::instance()->database()->transaction();
    for (Artist* artist : artists) {
        if (m_aborted) {
            Manager::instance()->database()->commit();
            return;
        }
        artist->controller()->loadData(Manager::instance()->mediaCenterInterface(), true);
        if (current % 20 == 0) {
            emit currentDir(artist->name());
        }
        emit progress(++current, max, m_progressMessageId);
        Manager::instance()->database()->add(artist, artistPaths.value(artist));
    }
    for (Album* album : albums) {
        if (m_aborted) {
            Manager::instance()->database()->commit();
            return;
        }
        album->controller()->loadData(Manager::instance()->mediaCenterInterface(), true);
        if (current % 20 == 0) {
            emit currentDir(album->artist() + "/" + album->title());
        }
        emit progress(++current, max, m_progressMessageId);
        Manager::instance()->database()->add(album, albumPaths.value(album));
    }
    Manager::instance()->database()->commit();

    QtConcurrent::blockingMapped(artistsFromDb, MusicFileSearcher::loadArtistData);
    QtConcurrent::blockingMapped(albumsFromDb, MusicFileSearcher::loadAlbumData);

    artists.append(artistsFromDb);
    albums.append(albumsFromDb);

    QMap<Artist*, MusicModelItem*> artistModelItems;
    for (Artist* artist : artists) {
        MusicModelItem* artistItem = Manager::instance()->musicModel()->appendChild(artist);
        artistModelItems.insert(artist, artistItem);
    }
    for (Album* album : albums) {
        MusicModelItem* artistItem = artistModelItems.value(album->artistObj(), 0);
        if (artistItem == nullptr) {
            qWarning() << "Artist item was not found for album" << album->path();
            continue;
        }
        artistItem->appendChild(album);
    }

    if (!m_aborted) {
        emit musicLoaded(m_progressMessageId);
    }
}

void MusicFileSearcher::abort()
{
    m_aborted = true;
}

Artist* MusicFileSearcher::loadArtistData(Artist* artist)
{
    artist->controller()->loadData(Manager::instance()->mediaCenterInterface(), false, false);
    return artist;
}

Album* MusicFileSearcher::loadAlbumData(Album* album)
{
    album->controller()->loadData(Manager::instance()->mediaCenterInterface(), false, false);
    return album;
}
