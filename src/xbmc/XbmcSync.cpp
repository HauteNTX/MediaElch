#include "XbmcSync.h"
#include "ui_XbmcSync.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

#include "globals/Manager.h"
#include "notifications/NotificationBox.h"
#include "settings/Settings.h"

// XbmcSync uses the Kodi JSON-RPC API
// See: https://kodi.wiki/view/JSON-RPC_API

XbmcSync::XbmcSync(KodiSettings& settings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::XbmcSync),
    m_settings{settings},
    m_allReady{false},
    m_aborted{false},
    m_syncType{SyncType::Clean},
    m_cancelRenameArtwork{false},
    m_renameArtworkInProgress{false},
    m_artworkWasRenamed{false},
    m_reloadTimeOut{2000},
    m_requestId{0}
{
    ui->setupUi(this);

    connect(&m_qnam, &QNetworkAccessManager::authenticationRequired, this, &XbmcSync::onAuthRequired);

    // clang-format off
    connect(ui->buttonSync,          &QAbstractButton::clicked, this, &XbmcSync::startSync);
    connect(ui->buttonClose,         &QAbstractButton::clicked, this, &XbmcSync::onButtonClose);
    connect(ui->radioUpdateContents, &QAbstractButton::clicked, this, &XbmcSync::onRadioContents);
    connect(ui->radioClean,          &QAbstractButton::clicked, this, &XbmcSync::onRadioClean);
    connect(ui->radioGetWatched,     &QAbstractButton::clicked, this, &XbmcSync::onRadioWatched);
    // clang-format on

    ui->progressBar->setVisible(false);
    onRadioContents();
}

XbmcSync::~XbmcSync()
{
    delete ui;
}

int XbmcSync::exec()
{
    m_renameArtworkInProgress = false;
    m_cancelRenameArtwork = false;
    m_artworkWasRenamed = false;
    ui->status->clear();
    ui->progressBar->setVisible(false);
    return QDialog::exec();
}

void XbmcSync::onButtonClose()
{
    if (m_renameArtworkInProgress) {
        m_cancelRenameArtwork = true;
        return;
    }
    reject();
}

void XbmcSync::reject()
{
    QDialog::reject();
    if (m_artworkWasRenamed) {
        Settings::instance()->setDataFiles(Settings::instance()->dataFilesFrodo());
        Settings::instance()->saveSettings();
        emit sigTriggerReload();
    } else {
        emit sigFinished();
    }
}

void XbmcSync::startSync()
{
    m_allReady = false;
    m_elements.clear();
    m_aborted = false;

    ui->progressBar->setVisible(false);

    m_moviesToSync.clear();
    m_concertsToSync.clear();
    m_tvShowsToSync.clear();
    m_episodesToSync.clear();

    m_xbmcMovies.clear();
    m_xbmcConcerts.clear();
    m_xbmcShows.clear();
    m_xbmcEpisodes.clear();

    m_moviesToRemove.clear();
    m_concertsToRemove.clear();
    m_tvShowsToRemove.clear();
    m_episodesToRemove.clear();

    for (Movie* movie : Manager::instance()->movieModel()->movies()) {
        if (movie->syncNeeded()) {
            m_moviesToSync.append(movie);
            if (m_syncType == SyncType::Contents) {
                updateFolderLastModified(movie);
            }
        }
    }

    for (Concert* concert : Manager::instance()->concertModel()->concerts()) {
        if (concert->syncNeeded()) {
            m_concertsToSync.append(concert);
        }
        if (m_syncType == SyncType::Contents) {
            updateFolderLastModified(concert);
        }
    }

    for (TvShow* show : Manager::instance()->tvShowModel()->tvShows()) {
        if (show->syncNeeded() && m_syncType == SyncType::Contents) {
            m_tvShowsToSync.append(show);
            updateFolderLastModified(show);
            continue;
        }
        /* @todo: Enable updating single episodes
         * Syncing single episodes is currently disabled:
         * XBMC doesn't pickup new episodes when VideoLibrary.Scan is called
         * so removing the whole show is needed.
         */
        for (TvShowEpisode* episode : show->episodes()) {
            if (episode->isDummy()) {
                continue;
            }

            if (episode->syncNeeded()) {
                if (m_syncType == SyncType::Contents) {
                    // m_episodesToSync.append(episode);
                    // updateFolderLastModified(episode);
                    m_tvShowsToSync.append(show);
                    break;
                } else if (m_syncType == SyncType::Watched) {
                    m_episodesToSync.append(episode);
                }
            }
        }
    }

    if (m_settings.xbmcHost().isEmpty() || m_settings.xbmcPort() == 0) {
        ui->status->setText(tr("Please fill in your Kodi host and port."));
        return;
    }

    if (ui->radioClean->isChecked()) {
        triggerClean();
        return;
    }

    QJsonObject limits;
    limits.insert("end", 100000);

    QJsonArray properties;
    properties.append(QString("file"));
    properties.append(QString("playcount"));
    properties.append(QString("lastplayed"));

    QJsonObject params;
    params.insert("limits", limits);
    params.insert("properties", properties);

    QJsonObject o;
    o.insert("jsonrpc", QString("2.0"));
    o.insert("params", params);

    if (!m_moviesToSync.isEmpty()) {
        m_elements.append(Element::Movies);
        o.insert("method", QString("VideoLibrary.GetMovies"));
        o.insert("id", ++m_requestId);
        QNetworkRequest request(xbmcUrl());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");
        QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, &XbmcSync::onMovieListFinished);
    }

    if (!m_concertsToSync.isEmpty()) {
        m_elements.append(Element::Concerts);
        o.insert("method", QString("VideoLibrary.GetMusicVideos"));
        o.insert("id", ++m_requestId);
        QNetworkRequest request(xbmcUrl());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");
        QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, &XbmcSync::onConcertListFinished);
    }

    if (!m_tvShowsToSync.isEmpty()) {
        m_elements.append(Element::TvShows);
        o.insert("method", QString("VideoLibrary.GetTvShows"));
        o.insert("id", ++m_requestId);
        QNetworkRequest request(xbmcUrl());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");
        QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, &XbmcSync::onTvShowListFinished);
    }

    if (!m_episodesToSync.isEmpty()) {
        m_elements.append(Element::Episodes);
        o.insert("method", QString("VideoLibrary.GetEpisodes"));
        o.insert("id", ++m_requestId);
        QNetworkRequest request(xbmcUrl());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");
        QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, &XbmcSync::onEpisodeListFinished);
    }

    if (m_moviesToSync.isEmpty() && m_concertsToSync.isEmpty() && m_tvShowsToSync.isEmpty()
        && m_episodesToSync.isEmpty()) {
        QTimer::singleShot(m_reloadTimeOut, this, &XbmcSync::triggerReload);
    } else {
        ui->status->setText(tr("Getting contents from Kodi"));
        ui->buttonSync->setEnabled(false);
    }
}

void XbmcSync::onMovieListFinished()
{
    auto reply = static_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "invalid response received";
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, tr("Network error"), reply->errorString());
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();
    QMapIterator<QString, QVariant> it(obj.value("result").toObject().toVariantMap());
    while (it.hasNext()) {
        it.next();
        if (it.key() == "movies" && !it.value().toList().empty()) {
            for (QVariant var : it.value().toList()) {
                if (var.toMap().value("movieid").toInt() == 0) {
                    continue;
                }
                m_xbmcMovies.insert(var.toMap().value("movieid").toInt(), parseXbmcDataFromMap(var.toMap()));
            }
        }
    }
    checkIfListsReady(Element::Movies);
}

void XbmcSync::onConcertListFinished()
{
    auto reply = static_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "invalid response received";
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, tr("Network error"), reply->errorString());
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();
    QMapIterator<QString, QVariant> it(obj.value("result").toObject().toVariantMap());
    while (it.hasNext()) {
        it.next();
        if (it.key() == "musicvideos" && !it.value().toList().empty()) {
            for (QVariant var : it.value().toList()) {
                if (var.toMap().value("musicvideoid").toInt() == 0) {
                    continue;
                }
                m_xbmcConcerts.insert(var.toMap().value("musicvideoid").toInt(), parseXbmcDataFromMap(var.toMap()));
            }
        }
    }
    checkIfListsReady(Element::Concerts);
}

void XbmcSync::onTvShowListFinished()
{
    auto reply = static_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "invalid response received";
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, tr("Network error"), reply->errorString());
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();
    QMapIterator<QString, QVariant> it(obj.value("result").toObject().toVariantMap());
    while (it.hasNext()) {
        it.next();
        if (it.key() == "tvshows" && !it.value().toList().empty()) {
            for (QVariant var : it.value().toList()) {
                if (var.toMap().value("tvshowid").toInt() == 0) {
                    continue;
                }
                m_xbmcShows.insert(var.toMap().value("tvshowid").toInt(), parseXbmcDataFromMap(var.toMap()));
            }
        }
    }
    checkIfListsReady(Element::TvShows);
}

void XbmcSync::onEpisodeListFinished()
{
    auto reply = static_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "invalid response received";
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, tr("Network error"), reply->errorString());
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();
    QMapIterator<QString, QVariant> it(obj.value("result").toObject().toVariantMap());
    while (it.hasNext()) {
        it.next();
        if (it.key() == "episodes" && !it.value().toList().empty()) {
            for (QVariant var : it.value().toList()) {
                if (var.toMap().value("episodeid").toInt() == 0) {
                    continue;
                }
                m_xbmcEpisodes.insert(var.toMap().value("episodeid").toInt(), parseXbmcDataFromMap(var.toMap()));
            }
        }
    }
    checkIfListsReady(Element::Episodes);
}

void XbmcSync::checkIfListsReady(Element element)
{
    QMutexLocker locker(&m_mutex);

    m_elements.removeOne(element);
    if (m_allReady || !m_elements.isEmpty() || m_aborted) {
        return;
    }

    m_allReady = true;

    if (m_syncType == SyncType::Contents) {
        setupItemsToRemove();
        if (!m_moviesToRemove.isEmpty() || !m_episodesToRemove.isEmpty() || !m_tvShowsToRemove.isEmpty()
            || !m_concertsToRemove.isEmpty()) {
            ui->progressBar->setMaximum(m_moviesToRemove.count() + m_episodesToRemove.count()
                                        + m_tvShowsToRemove.count() + m_concertsToRemove.count());
            ui->progressBar->setValue(0);
            ui->progressBar->setVisible(true);
        }
        removeItems();
    } else if (m_syncType == SyncType::Watched) {
        updateWatched();
    }
}

void XbmcSync::setupItemsToRemove()
{
    for (Movie* movie : m_moviesToSync) {
        movie->setSyncNeeded(false);
        int id = findId(movie->files(), m_xbmcMovies);
        if (id > 0) {
            m_moviesToRemove.append(id);
        }
    }

    for (Concert* concert : m_concertsToSync) {
        concert->setSyncNeeded(false);
        int id = findId(concert->files(), m_xbmcConcerts);
        if (id > 0) {
            m_concertsToRemove.append(id);
        }
    }

    for (TvShow* show : m_tvShowsToSync) {
        show->setSyncNeeded(false);
        for (TvShowEpisode* episode : show->episodes()) {
            episode->setSyncNeeded(false);
        }
        QString showDir = show->dir();
        if (showDir.contains("/") && !showDir.endsWith("/")) {
            showDir.append("/");
        } else if (!showDir.contains("/") && !showDir.endsWith("\\")) {
            showDir.append("\\");
        }
        int id = findId(QStringList() << showDir, m_xbmcShows);
        if (id > 0) {
            m_tvShowsToRemove.append(id);
        }
    }

    for (TvShowEpisode* episode : m_episodesToSync) {
        episode->setSyncNeeded(false);
        int id = findId(episode->files(), m_xbmcEpisodes);
        if (id > 0) {
            m_episodesToRemove.append(id);
        }
    }
}

void XbmcSync::removeItems()
{
    QJsonObject o;
    o.insert("jsonrpc", QString("2.0"));
    o.insert("id", ++m_requestId);

    if (!m_moviesToRemove.isEmpty()) {
        ui->status->setText(tr("Removing movies from database"));
        int id = m_moviesToRemove.takeFirst();
        QJsonObject params;
        params.insert("movieid", id);
        o.insert("params", params);
        o.insert("method", QString("VideoLibrary.RemoveMovie"));
        QNetworkRequest request(xbmcUrl());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");
        QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, &XbmcSync::onRemoveFinished);
        return;
    }

    if (!m_concertsToRemove.isEmpty()) {
        ui->status->setText(tr("Removing concerts from database"));
        int id = m_concertsToRemove.takeFirst();
        QJsonObject params;
        params.insert("musicvideoid", id);
        o.insert("params", params);
        o.insert("method", QString("VideoLibrary.RemoveMusicVideo"));
        QNetworkRequest request(xbmcUrl());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");
        QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, &XbmcSync::onRemoveFinished);
        return;
    }

    if (!m_tvShowsToRemove.isEmpty()) {
        ui->status->setText(tr("Removing TV shows from database"));
        int id = m_tvShowsToRemove.takeFirst();
        QJsonObject params;
        params.insert("tvshowid", id);
        o.insert("params", params);
        o.insert("method", QString("VideoLibrary.RemoveTVShow"));
        QNetworkRequest request(xbmcUrl());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");
        QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, &XbmcSync::onRemoveFinished);
        return;
    }

    if (!m_episodesToRemove.isEmpty()) {
        ui->status->setText(tr("Removing episodes from database"));
        int id = m_episodesToRemove.takeFirst();

        QJsonObject params;
        params.insert("episodeid", id);
        o.insert("params", params);
        o.insert("method", QString("VideoLibrary.RemoveEpisode"));
        QNetworkRequest request(xbmcUrl());
        request.setRawHeader("Content-Type", "application/json");
        request.setRawHeader("Accept", "application/json");
        QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, &XbmcSync::onRemoveFinished);
        return;
    }

    QTimer::singleShot(m_reloadTimeOut, this, &XbmcSync::triggerReload);
}

void XbmcSync::onRemoveFinished()
{
    auto reply = static_cast<QNetworkReply*>(sender());
    if (reply) {
        reply->deleteLater();
    }

    ui->progressBar->setValue(ui->progressBar->maximum()
                              - (m_moviesToRemove.count() + m_episodesToRemove.count() + m_tvShowsToRemove.count()
                                    + m_concertsToRemove.count()));
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    if (!m_moviesToRemove.isEmpty() || !m_concertsToRemove.isEmpty() || !m_tvShowsToRemove.isEmpty()
        || !m_episodesToRemove.isEmpty()) {
        removeItems();
    } else {
        QTimer::singleShot(m_reloadTimeOut, this, &XbmcSync::triggerReload);
    }
}

void XbmcSync::triggerReload()
{
    ui->status->setText(tr("Trigger scan for new items"));

    QJsonObject o;
    o.insert("jsonrpc", QString("2.0"));
    o.insert("method", QString("VideoLibrary.Scan"));
    o.insert("id", ++m_requestId);

    QNetworkRequest request(xbmcUrl());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, &XbmcSync::onScanFinished);
}

void XbmcSync::onScanFinished()
{
    ui->status->setText(tr("Finished. Kodi is now loading your updated items."));
    ui->buttonSync->setEnabled(true);
}

void XbmcSync::triggerClean()
{
    QJsonObject o;
    o.insert("jsonrpc", QString("2.0"));
    o.insert("method", QString("VideoLibrary.Clean"));
    o.insert("id", ++m_requestId);

    QNetworkRequest request(xbmcUrl());
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Accept", "application/json");
    QNetworkReply* reply = m_qnam.post(request, QJsonDocument(o).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, &XbmcSync::onCleanFinished);
}

void XbmcSync::onCleanFinished()
{
    ui->status->setText(tr("Finished. Kodi is now cleaning your database."));
    ui->buttonSync->setEnabled(true);
}

void XbmcSync::updateWatched()
{
    for (Movie* movie : m_moviesToSync) {
        int id = findId(movie->files(), m_xbmcMovies);
        if (id > 0) {
            movie->blockSignals(true);
            movie->setWatched(m_xbmcMovies.value(id).playCount > 0);
            movie->setPlayCount(m_xbmcMovies.value(id).playCount);
            movie->setLastPlayed(m_xbmcMovies.value(id).lastPlayed);
            movie->blockSignals(false);
        } else {
            qDebug() << "Movie not found" << movie->name();
        }
        movie->setSyncNeeded(false);
    }

    for (Concert* concert : m_concertsToSync) {
        int id = findId(concert->files(), m_xbmcConcerts);
        if (id > 0) {
            concert->blockSignals(true);
            concert->setWatched(m_xbmcConcerts.value(id).playCount > 0);
            concert->setPlayCount(m_xbmcConcerts.value(id).playCount);
            concert->setLastPlayed(m_xbmcConcerts.value(id).lastPlayed);
            concert->blockSignals(false);
        } else {
            qDebug() << "Concert not found" << concert->name();
        }
        concert->setSyncNeeded(false);
    }

    for (TvShowEpisode* episode : m_episodesToSync) {
        int id = findId(episode->files(), m_xbmcEpisodes);
        if (id > 0) {
            episode->blockSignals(true);
            episode->setPlayCount(m_xbmcEpisodes.value(id).playCount);
            episode->setLastPlayed(m_xbmcEpisodes.value(id).lastPlayed);
            episode->blockSignals(false);
        } else {
            qDebug() << "Episode not found" << episode->name();
        }
        episode->setSyncNeeded(false);
    }

    ui->status->setText(tr("Finished. Your items play count and last played date have been updated."));
    ui->buttonSync->setEnabled(true);
}

int XbmcSync::findId(const QStringList& files, const QMap<int, XbmcData>& items)
{
    if (files.isEmpty()) {
        return -1;
    }

    QVector<int> matches;
    int level = 0;

    do {
        matches.clear();
        QMapIterator<int, XbmcData> it(items);
        while (it.hasNext()) {
            it.next();
            QString file = it.value().file;
            QStringList xbmcFiles;
            if (file.startsWith("stack://")) {
                xbmcFiles << file.mid(8).split(" , ");
            } else {
                xbmcFiles << file;
            }

            if (compareFiles(files, xbmcFiles, level)) {
                matches.append(it.key());
            }
        }
    } while (matches.count() > 1 && level++ < 4);

    if (matches.count() == 1) {
        return matches.at(0);
    } else if (matches.count() == 0) {
        return 0;
    } else {
        return -1;
    }
}

bool XbmcSync::compareFiles(const QStringList& files, const QStringList& xbmcFiles, const int& level)
{
    if (files.count() == 1 && xbmcFiles.count() == 1) {
        QStringList file = splitFile(files.at(0));
        QStringList xbmcFile = splitFile(xbmcFiles.at(0));
        for (int i = 0; i <= level; ++i) {
            if (file.isEmpty() || xbmcFile.isEmpty()) {
                return false;
            }
            if (QString::compare(file.takeLast(), xbmcFile.takeLast(), Qt::CaseInsensitive) != 0) {
                return false;
            }
        }
        return true;
    } else if (files.count() == xbmcFiles.count()) {
        // construct a new stack
        QStringList stack;
        QStringList xbmcStack;
        for (QString file : xbmcFiles) {
            QStringList parts = splitFile(file);
            if (parts.count() < level) {
                return false;
            }
            QStringList partsNew;
            for (int i = 0; i <= level; ++i) {
                partsNew << parts.takeLast();
            }
            xbmcStack << partsNew.join("/");
        }

        for (QString file : files) {
            QStringList parts = splitFile(file);
            if (parts.count() < level) {
                return false;
            }
            QStringList partsNew;
            for (int i = 0; i <= level; ++i) {
                partsNew << parts.takeLast();
            }
            stack << partsNew.join("/");
        }

        qSort(stack);
        qSort(xbmcStack);

        return (stack == xbmcStack);
    }
    return false;
}

QStringList XbmcSync::splitFile(const QString& file)
{
    // Windows file names must not contain /
    if (file.contains("/")) {
        return file.split("/");
    } else {
        return file.split("\\");
    }
}

void XbmcSync::onRadioContents()
{
    ui->labelContents->setVisible(true);
    ui->labelWatched->setVisible(false);
    ui->labelClean->setVisible(false);
    m_syncType = SyncType::Contents;
}

void XbmcSync::onRadioClean()
{
    ui->labelContents->setVisible(false);
    ui->labelWatched->setVisible(false);
    ui->labelClean->setVisible(true);
    m_syncType = SyncType::Clean;
}

void XbmcSync::onRadioWatched()
{
    ui->labelContents->setVisible(false);
    ui->labelWatched->setVisible(true);
    ui->labelClean->setVisible(false);
    m_syncType = SyncType::Watched;
}

XbmcSync::XbmcData XbmcSync::parseXbmcDataFromMap(QMap<QString, QVariant> map)
{
    XbmcData d;
    d.file = map.value("file").toString().normalized(QString::NormalizationForm_C);
    d.lastPlayed = map.value("lastplayed").toDateTime();
    d.playCount = map.value("playcount").toInt();
    return d;
}

void XbmcSync::updateFolderLastModified(Movie* movie)
{
    if (movie->files().isEmpty()) {
        return;
    }

    QFileInfo fi(movie->files().first());
    QDir dir = fi.dir();
    if (movie->discType() == DiscType::BluRay || movie->discType() == DiscType::Dvd) {
        dir = fi.dir();
        dir.cdUp();
    }
    QFile file(dir.absolutePath() + "/.update");
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
        file.remove();
    }
}

void XbmcSync::updateFolderLastModified(Concert* concert)
{
    if (concert->files().isEmpty()) {
        return;
    }

    QFileInfo fi(concert->files().first());
    QDir dir = fi.dir();
    if (concert->discType() == DiscType::BluRay || concert->discType() == DiscType::Dvd) {
        dir = fi.dir();
        dir.cdUp();
    }
    QFile file(dir.absolutePath() + "/.update");
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
        file.remove();
    }
}

void XbmcSync::updateFolderLastModified(TvShow* show)
{
    QDir dir(show->dir());
    QFile file(dir.absolutePath() + "/.update");
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
        file.remove();
    }
}

void XbmcSync::updateFolderLastModified(TvShowEpisode* episode)
{
    if (episode->files().isEmpty()) {
        return;
    }

    QFileInfo fi(episode->files().first());
    QDir dir = fi.dir();
    QFile file(dir.absolutePath() + "/.update");
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
        file.remove();
    }
}

void XbmcSync::onAuthRequired(QNetworkReply* reply, QAuthenticator* authenticator)
{
    Q_UNUSED(reply);

    authenticator->setUser(m_settings.xbmcUser());
    authenticator->setPassword(m_settings.xbmcPassword());
}

QUrl XbmcSync::xbmcUrl()
{
    QString url = "http://";
    if (!m_settings.xbmcUser().isEmpty()) {
        url.append(m_settings.xbmcUser());
        if (!m_settings.xbmcPassword().isEmpty()) {
            url.append(":" + m_settings.xbmcPassword());
        }
        url.append("@");
    }
    url.append(QString("%1:%2/jsonrpc").arg(m_settings.xbmcHost()).arg(m_settings.xbmcPort()));
    return QUrl{url};
}
