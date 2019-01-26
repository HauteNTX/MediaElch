#pragma once

#include "data/Movie.h"

#include <QDialog>
#include <QTableWidgetItem>

namespace Ui {
class MovieListDialog;
}

/**
 * @brief The MovieListDialog class
 * Displays a list of movies (for the sets widget)
 */
class MovieListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MovieListDialog(QWidget *parent = nullptr);
    ~MovieListDialog() override;
    QList<Movie *> selectedMovies();
    static MovieListDialog *instance(QWidget *parent = nullptr);

public slots:
    int exec() override;
    int execWithoutGenre(QString genre);
    int execWithoutCertification(Certification certification);

private slots:
    void onAddMovies();
    void onFilterEdited(QString text);

private:
    Ui::MovieListDialog *ui;
    QList<Movie *> m_selectedMovies;
    void reposition();
};
