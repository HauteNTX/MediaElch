#include "ConcertInfoWidget.h"
#include "ui_ConcertInfoWidget.h"

#include "globals/LocaleStringCompare.h"
#include "globals/Manager.h"

// Each change event listener requires the concert to be valid. This is a marco to avoid repitition.
// do {} while() is used to force a semicolon after the use of this macro.
#define ME_REQUIRE_CONCERT_OR_RETURN                                                                                   \
    do {                                                                                                               \
        if (!m_concertController || !m_concertController->concert()) {                                                 \
            return;                                                                                                    \
        }                                                                                                              \
    } while (false)

ConcertInfoWidget::ConcertInfoWidget(QWidget* parent) : QWidget(parent), ui(std::make_unique<Ui::ConcertInfoWidget>())
{
    ui->setupUi(this);

    ui->badgeWatched->setBadgeType(Badge::Type::BadgeInfo);

    // clang-format off
    connect(ui->name,          &QLineEdit::textChanged,         this, &ConcertInfoWidget::onConcertNameChanged);
    connect(ui->name,          &QLineEdit::textEdited,          this, &ConcertInfoWidget::onNameChange);
    connect(ui->artist,        &QLineEdit::textEdited,          this, &ConcertInfoWidget::onArtistChange);
    connect(ui->album,         &QLineEdit::textEdited,          this, &ConcertInfoWidget::onAlbumChange);
    connect(ui->tagline,       &QLineEdit::textEdited,          this, &ConcertInfoWidget::onTaglineChange);
    connect(ui->rating,        SIGNAL(valueChanged(double)),    this, SLOT(onRatingChange(double)));
    connect(ui->trailer,       &QLineEdit::textEdited,          this, &ConcertInfoWidget::onTrailerChange);
    connect(ui->runtime,       SIGNAL(valueChanged(int)),       this, SLOT(onRuntimeChange(int)));
    connect(ui->playcount,     SIGNAL(valueChanged(int)),       this, SLOT(onPlayCountChange(int)));
    connect(ui->certification, &QComboBox::editTextChanged,     this, &ConcertInfoWidget::onCertificationChange);
    connect(ui->badgeWatched,  &Badge::clicked,                 this, &ConcertInfoWidget::onWatchedClicked);
    connect(ui->released,      &QDateTimeEdit::dateChanged,     this, &ConcertInfoWidget::onReleasedChange);
    connect(ui->lastPlayed,    &QDateTimeEdit::dateTimeChanged, this, &ConcertInfoWidget::onLastWatchedChange);
    connect(ui->overview,      &QTextEdit::textChanged,         this, &ConcertInfoWidget::onOverviewChange);
    // clang-format on
}

// Do NOT move the destructor into the header or unique_ptr requires a
// complete type of UI::ConcertInfoWidget
ConcertInfoWidget::~ConcertInfoWidget() = default;

void ConcertInfoWidget::setConcertController(ConcertController* controller)
{
    m_concertController = controller;
}

void ConcertInfoWidget::updateConcertInfo()
{
    if (!m_concertController || !m_concertController->concert()) {
        qDebug() << "My concert is invalid";
        return;
    }

    ui->rating->blockSignals(true);
    ui->runtime->blockSignals(true);
    ui->playcount->blockSignals(true);
    ui->certification->blockSignals(true);
    ui->released->blockSignals(true);
    ui->lastPlayed->blockSignals(true);
    ui->overview->blockSignals(true);

    clear();

    ui->files->setText(m_concertController->concert()->files().join(", "));
    ui->files->setToolTip(m_concertController->concert()->files().join("\n"));
    ui->name->setText(m_concertController->concert()->name());
    ui->artist->setText(m_concertController->concert()->artist());
    ui->album->setText(m_concertController->concert()->album());
    ui->tagline->setText(m_concertController->concert()->tagline());
    ui->rating->setValue(m_concertController->concert()->rating());
    ui->released->setDate(m_concertController->concert()->released());
    ui->runtime->setValue(static_cast<int>(m_concertController->concert()->runtime().count()));
    ui->trailer->setText(m_concertController->concert()->trailer().toString());
    ui->playcount->setValue(m_concertController->concert()->playcount());
    ui->lastPlayed->setDateTime(m_concertController->concert()->lastPlayed());
    ui->overview->setPlainText(m_concertController->concert()->overview());
    ui->badgeWatched->setActive(m_concertController->concert()->watched());

    QStringList certifications;
    certifications.append("");
    for (const Concert* concert : Manager::instance()->concertModel()->concerts()) {
        if (!certifications.contains(concert->certification().toString()) && concert->certification().isValid()) {
            certifications.append(concert->certification().toString());
        }
    }
    qSort(certifications.begin(), certifications.end(), LocaleStringCompare());
    ui->certification->addItems(certifications);
    ui->certification->setCurrentIndex(
        certifications.indexOf(m_concertController->concert()->certification().toString()));

    ui->rating->blockSignals(false);
    ui->runtime->blockSignals(false);
    ui->playcount->blockSignals(false);
    ui->certification->blockSignals(false);
    ui->released->blockSignals(false);
    ui->lastPlayed->blockSignals(false);
    ui->overview->blockSignals(false);

    ui->rating->setEnabled(
        Manager::instance()->mediaCenterInterfaceConcert()->hasFeature(MediaCenterFeatures::EditConcertRating));
    ui->tagline->setEnabled(
        Manager::instance()->mediaCenterInterfaceConcert()->hasFeature(MediaCenterFeatures::EditConcertTagline));
    ui->certification->setEnabled(
        Manager::instance()->mediaCenterInterfaceConcert()->hasFeature(MediaCenterFeatures::EditConcertCertification));
    ui->trailer->setEnabled(
        Manager::instance()->mediaCenterInterfaceConcert()->hasFeature(MediaCenterFeatures::EditConcertTrailer));
    ui->badgeWatched->setEnabled(
        Manager::instance()->mediaCenterInterfaceConcert()->hasFeature(MediaCenterFeatures::EditConcertWatched));
}

void ConcertInfoWidget::setRuntime(std::chrono::minutes runtime)
{
    ui->runtime->setValue(static_cast<int>(runtime.count()));
}

void ConcertInfoWidget::clear()
{
    ui->certification->clear();
    ui->files->clear();
    ui->name->clear();
    ui->artist->clear();
    ui->album->clear();
    ui->tagline->clear();
    ui->rating->clear();
    ui->released->setDate(QDate::currentDate());
    ui->runtime->clear();
    ui->trailer->clear();
    ui->playcount->clear();
    ui->lastPlayed->setDateTime(QDateTime::currentDateTime());
    ui->overview->clear();
}

void ConcertInfoWidget::onConcertNameChanged(QString concertName)
{
    emit concertNameChanged(concertName);
}

/**
 * @brief Marks the concert as changed when the name has changed
 */
void ConcertInfoWidget::onNameChange(QString text)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setName(text);
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the artist has changed
 */
void ConcertInfoWidget::onArtistChange(QString text)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setArtist(text);
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the album has changed
 */
void ConcertInfoWidget::onAlbumChange(QString text)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setAlbum(text);
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the tagline has changed
 */
void ConcertInfoWidget::onTaglineChange(QString text)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setTagline(text);
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the rating has changed
 */
void ConcertInfoWidget::onRatingChange(double value)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setRating(value);
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the release date has changed
 */
void ConcertInfoWidget::onReleasedChange(QDate date)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setReleased(date);
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the runtime has changed
 */
void ConcertInfoWidget::onRuntimeChange(const int value)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setRuntime(std::chrono::minutes(value));
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the certification has changed
 */
void ConcertInfoWidget::onCertificationChange(QString text)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setCertification(Certification(text));
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the trailer has changed
 */
void ConcertInfoWidget::onTrailerChange(QString text)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setTrailer(text);
    emit infoChanged();
}

void ConcertInfoWidget::onWatchedClicked()
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    const bool active = !ui->badgeWatched->isActive();
    ui->badgeWatched->setActive(active);
    m_concertController->concert()->setWatched(active);

    if (active) {
        if (m_concertController->concert()->playcount() < 1) {
            ui->playcount->setValue(1);
        }
        if (!m_concertController->concert()->lastPlayed().isValid()) {
            ui->lastPlayed->setDateTime(QDateTime::currentDateTime());
        }
    }
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the play count has changed
 */
void ConcertInfoWidget::onPlayCountChange(int value)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setPlayCount(value);
    ui->badgeWatched->setActive(value > 0);
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the last watched date has changed
 */
void ConcertInfoWidget::onLastWatchedChange(QDateTime dateTime)
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setLastPlayed(dateTime);
    emit infoChanged();
}

/**
 * @brief Marks the concert as changed when the overview has changed
 */
void ConcertInfoWidget::onOverviewChange()
{
    ME_REQUIRE_CONCERT_OR_RETURN;
    m_concertController->concert()->setOverview(ui->overview->toPlainText());
    emit infoChanged();
}
