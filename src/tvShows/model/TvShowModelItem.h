#pragma once

#include "data/SeasonNumber.h"
#include "globals/Globals.h"
#include "tvShows/model/TvShowBaseModelItem.h"

#include <QList>
#include <QObject>
#include <QVariant>

class TvShow;
class TvShowEpisode;
class EpisodeModelItem;
class SeasonModelItem;
class TvShowRootModelItem;

class TvShowModelItem : public TvShowBaseModelItem
{
    Q_OBJECT

public:
    explicit TvShowModelItem(TvShowRootModelItem& parent) : m_parentItem{parent} {}
    ~TvShowModelItem() override;

    QVariant data(int column) const override;

    TvShowBaseModelItem* child(int number) const override;
    int childCount() const override;
    bool removeChildren(int position, int count) override;

    TvShowBaseModelItem* parent() const override;
    int indexInParent() const override;

    TvShow* tvShow() override;
    const TvShow* tvShow() const override;

    TvShowType type() const override;

    SeasonModelItem* seasonAtIndex(int number) const;
    SeasonModelItem* appendSeason(SeasonNumber seasonNumber, QString season, TvShow* show);
    const QList<SeasonModelItem*>& seasons() const;

    void setTvShow(TvShow* show);

signals:
    void sigChanged(TvShowModelItem*, SeasonModelItem*, EpisodeModelItem*);

private slots:
    void onEpisodeChanged(SeasonModelItem* seasonItem, EpisodeModelItem* episodeItem);

private:
    QList<SeasonModelItem*> m_children;

    TvShowRootModelItem& m_parentItem;
    TvShow* m_tvShow = nullptr;
};
