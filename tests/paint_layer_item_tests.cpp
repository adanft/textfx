#include "app/qt/PaintLayerItem.h"

#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

using namespace textfx;

namespace {
QVariantMap stroke(QString color = QStringLiteral("ff0000ff")) {
  return {{QStringLiteral("color"), std::move(color)},
          {QStringLiteral("size"), 6.0},
          {QStringLiteral("opacity"), 1.0},
          {QStringLiteral("points"),
           QVariantList{QVariantList{4.0, 4.0}, QVariantList{28.0, 4.0}}}};
}

void paintItem(PaintLayerItem &item) {
  QImage image(32, 16, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  item.paint(&painter);
}
} // namespace

class PaintLayerItemTests final : public QObject {
  Q_OBJECT

private slots:
  void init() { qunsetenv("TEXTFX_RENDER_STATS"); }
  void cleanup() { qunsetenv("TEXTFX_RENDER_STATS"); }

  void paintDoesNotMutateQObjectFacingState() {
    PaintLayerItem item;
    item.setStrokes({stroke()});
    QSignalSpy countSpy(&item, &PaintLayerItem::lastPaintedStrokeCountChanged);
    QSignalSpy revisionSpy(&item, &PaintLayerItem::paintRevisionChanged);
    const int count = item.lastPaintedStrokeCount();
    const int revision = item.paintRevision();

    paintItem(item);

    QCOMPARE(item.lastPaintedStrokeCount(), count);
    QCOMPARE(item.paintRevision(), revision);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(revisionSpy.count(), 0);
  }

  void schedulingPublishesDeterministicCounters() {
    PaintLayerItem item;
    QCOMPARE(item.paintRevision(), 0);
    QCOMPARE(item.lastPaintedStrokeCount(), 0);

    item.setStrokes({stroke(), stroke(QStringLiteral("00ff00ff"))});
    QCOMPARE(item.paintRevision(), 1);
    QCOMPARE(item.liveStrokeCount(), 2);
    QCOMPARE(item.lastPaintedStrokeCount(), 2);

    item.schedulePaint();
    QCOMPARE(item.paintRevision(), 2);
    QCOMPARE(item.lastPaintedStrokeCount(), 2);
    paintItem(item);
    QCOMPARE(item.paintRevision(), 2);
  }

  void renderStatsAreDisabledByDefault() {
    PaintLayerItem item;
    QVERIFY(!item.renderStatsEnabledForTesting());
    paintItem(item);
    QCOMPARE(item.renderStatsForTesting().value(QStringLiteral("paintCount"))
                 .toULongLong(),
             0);
  }

  void renderStatsRecordAndReportWhenEnabled() {
    qputenv("TEXTFX_RENDER_STATS", "1");
    PaintLayerItem item;
    item.setStrokes({stroke()});
    QVERIFY(item.renderStatsEnabledForTesting());

    paintItem(item);
    const QVariantMap stats = item.renderStatsForTesting();
    QCOMPARE(stats.value(QStringLiteral("paintCount")).toULongLong(), 1);
    QCOMPARE(stats.value(QStringLiteral("latestStrokeCount")).toInt(), 1);
    QVERIFY(stats.value(QStringLiteral("totalNanoseconds")).toULongLong() >=
            stats.value(QStringLiteral("maximumNanoseconds")).toULongLong());

    item.reportRenderStatsForTesting();
    QCOMPARE(item.renderStatsForTesting().value(QStringLiteral("paintCount"))
                 .toULongLong(),
             1);
  }
};

QTEST_MAIN(PaintLayerItemTests)
#include "paint_layer_item_tests.moc"
