#ifndef MAKEMKVDIALOG_H
#define MAKEMKVDIALOG_H

#include <QDialog>
#include <QPointer>
#include "downloads/MakeMkvCon.h"
#include "movies/Movie.h"

namespace Ui {
class MakeMkvDialog;
}

class MakeMkvDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MakeMkvDialog(QWidget *parent = nullptr);
    ~MakeMkvDialog();

public slots:
    int exec();
    void reject();
    void accept();

private slots:
    void onGotDrives(QMap<int, QString> drives);
    void onScanDrive();
    void onScanFinished(QString title, QMap<int, MakeMkvCon::Track> tracks);
    void onImportTracks();
    void onImportComplete();
    void onMovieChosen();
    void onLoadDone(Movie *movie);
    void onImport();
    void onDiscBackedUp();
    void onTrackImported(int trackId);
    void onImportProgress(int value, int max);

private:
    Ui::MakeMkvDialog *ui;
    MakeMkvCon *m_makeMkvCon;
    QPointer<Movie> m_movie;
    QString m_title;
    QMap<int, QString> m_tracks;
    bool m_importComplete;
    QString m_importDir;

    void setDefaults();
    void storeDefaults();
    void importFinished();
};

#endif // MAKEMKVDIALOG_H
