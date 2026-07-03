#include "core/DocumentModel.h"
#include "qt_test_helpers.h"
#include "render/RenderGraph.h"
#include "ui/OutlinedTextItem.h"

#include <QColor>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantList>

#include <cmath>
#include <functional>
#include <vector>

using namespace textfx;
using namespace textfx::test;

class OutlinedTextItemTests final : public QObject {
  Q_OBJECT

private slots:
  void wrapsInDocumentUnitsAcrossRenderScale() {
    auto linesAtScale = [](qreal scale) {
      OutlinedTextItem item;
      item.setWidth(96 * scale);
      item.setHeight(120 * scale);
      item.setText(QStringLiteral("DICHO QUE ME"));
      item.setPixelSize(24);
      item.setLetterSpacing(1);
      item.setLineSpacing(2);
      item.setOutlineSize(4);
      item.setRenderScale(scale);
      return item.wrappedLinesForTesting();
    };

    const QStringList expected = linesAtScale(1.0);
    QVERIFY(expected.size() >= 2);
    QCOMPARE(linesAtScale(0.75), expected);
    QCOMPARE(linesAtScale(1.5), expected);
  }

  void longWordsDoNotSplitArbitrarily() {
    OutlinedTextItem item;
    item.setWidth(80);
    item.setHeight(120);
    item.setText(QStringLiteral("Supercalifragilisticexpialidocious"));
    item.setPixelSize(20);

    const QStringList lines = item.wrappedLinesForTesting();
    QCOMPARE(lines,
             QStringList{QStringLiteral("Supercalifragilisticexpialidocious")});
  }

  void spacesStillWrapAcrossLines() {
    OutlinedTextItem item;
    item.setWidth(110);
    item.setHeight(120);
    item.setText(QStringLiteral("Alpha Beta Gamma"));
    item.setPixelSize(20);

    const QStringList lines = item.wrappedLinesForTesting();
    QVERIFY2(lines.size() >= 2, qPrintable(lines.join(QStringLiteral("|"))));
    QCOMPARE(lines.join(QStringLiteral(" ")).simplified(),
             QStringLiteral("Alpha Beta Gamma"));
  }

  void overflowTracksTextThatExceedsBox() {
    OutlinedTextItem item;
    item.setWidth(60);
    item.setHeight(28);
    item.setText(QStringLiteral("Supercalifragilisticexpialidocious"));
    item.setPixelSize(20);

    QVERIFY(item.overflow());

    item.setWidth(600);
    item.setHeight(120);
    QVERIFY(!item.overflow());
  }

  void textChangesRequestPaintRefresh() {
    OutlinedTextItem item;
    item.setWidth(180);
    item.setHeight(80);

    QSignalSpy textChanged(&item, &OutlinedTextItem::textChanged);
    QSignalSpy paintRequested(
        &item, &OutlinedTextItem::paintRequestRevisionChangedForTesting);
    QSignalSpy layoutChanged(&item, &OutlinedTextItem::editLayoutMetricsChanged);

    item.setText(QStringLiteral("Live"));

    QCOMPARE(textChanged.count(), 1);
    QCOMPARE(paintRequested.count(), 1);
    QCOMPARE(layoutChanged.count(), 1);
    QCOMPARE(item.paintRequestRevisionForTesting(), 1);

    item.setText(QStringLiteral("Live"));
    QCOMPARE(textChanged.count(), 1);
    QCOMPARE(paintRequested.count(), 1);
    QCOMPARE(layoutChanged.count(), 1);
    QCOMPARE(item.paintRequestRevisionForTesting(), 1);

    item.setText(QStringLiteral("Live!"));
    QCOMPARE(textChanged.count(), 2);
    QCOMPARE(paintRequested.count(), 2);
    QCOMPARE(layoutChanged.count(), 2);
    QCOMPARE(item.paintRequestRevisionForTesting(), 2);
  }

  void usesOutlineSizeWhenPainting() {
    auto render = [](qreal outlineSize) {
      OutlinedTextItem item;
      item.setWidth(180);
      item.setHeight(80);
      item.setText(QStringLiteral("Outline"));
      item.setPixelSize(42);
      item.setColor(Qt::black);
      item.setOutlineColor(Qt::red);
      item.setOutlineSize(outlineSize);

      QImage image(180, 80, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };
    auto redPixels = [](const QImage &image) {
      int count = 0;
      for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
          const QColor c = image.pixelColor(x, y);
          if (c.red() > 80 && c.green() < 40 && c.blue() < 40 && c.alpha() > 0)
            ++count;
        }
      }
      return count;
    };

    QVERIFY(redPixels(render(8)) > redPixels(render(1)));
  }

  void legacyOddOutlineSizeKeepsCenteredStrokeSemantics() {
    auto render = [](const QVariantList &layers) {
      OutlinedTextItem item;
      item.setWidth(180);
      item.setHeight(80);
      item.setText(QStringLiteral("Outline"));
      item.setPixelSize(42);
      item.setColor(Qt::black);
      item.setOutlineColor(Qt::red);
      item.setOutlineSize(7);
      item.setOutlineLayers(layers);

      QImage image(180, 80, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };

    auto imagesEqual = [](const QImage &a, const QImage &b) {
      if (a.size() != b.size() || a.format() != b.format())
        return false;
      for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
          if (a.pixelColor(x, y) != b.pixelColor(x, y))
            return false;
        }
      }
      return true;
    };

    const auto legacy = render({});
    const auto roundedFallbackLayer = render({QVariantMap{{QStringLiteral("enabled"), true},
                                                         {QStringLiteral("color"), QStringLiteral("#ff0000")},
                                                         {QStringLiteral("size"), 4}}});
    const auto explicitSeven = render({QVariantMap{{QStringLiteral("enabled"), true},
                                                  {QStringLiteral("color"), QStringLiteral("#ff0000")},
                                                  {QStringLiteral("size"), 7}}});

    QVERIFY(!imagesEqual(legacy, roundedFallbackLayer));
    QVERIFY(!imagesEqual(legacy, explicitSeven));

    OutlinedTextItem legacyItem;
    legacyItem.setOutlineSize(7);
    QCOMPARE(legacyItem.effectiveMaxOutlineSizeForTesting(), 7.0);

    OutlinedTextItem explicitItem;
    explicitItem.setOutlineLayers({QVariantMap{{QStringLiteral("enabled"), true},
                                                {QStringLiteral("color"), QStringLiteral("#ff0000")},
                                                {QStringLiteral("size"), 7}}});
    QCOMPARE(explicitItem.effectiveMaxOutlineSizeForTesting(), 14.0);
  }

  void paintsMultipleOutlineLayersStackedByUiOrder() {
    OutlinedTextItem item;
    item.setWidth(220);
    item.setHeight(90);
    item.setText(QStringLiteral("Stack"));
    item.setPixelSize(46);
    item.setColor(Qt::black);
    item.setOutlineLayers({QVariantMap{{QStringLiteral("enabled"), true},
                                       {QStringLiteral("color"), QStringLiteral("#ffffff")},
                                       {QStringLiteral("size"), 9}},
                           QVariantMap{{QStringLiteral("enabled"), true},
                                       {QStringLiteral("color"), QStringLiteral("#ff0000")},
                                       {QStringLiteral("size"), 11}}});

    QImage image(220, 90, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    item.paint(&painter);

    int redPixels = 0;
    int whitePixels = 0;
    int blackPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
        const QColor c = image.pixelColor(x, y);
        if (c.red() > 100 && c.green() < 80 && c.blue() < 80 && c.alpha() > 0)
          ++redPixels;
        if (c.red() > 180 && c.green() > 180 && c.blue() > 180 && c.alpha() > 0)
          ++whitePixels;
        if (c.red() < 40 && c.green() < 40 && c.blue() < 40 && c.alpha() > 0)
          ++blackPixels;
      }
    }
    QVERIFY2(redPixels > 300,
             qPrintable(QStringLiteral("redPixels=%1").arg(redPixels)));
    QVERIFY2(whitePixels > 0,
             qPrintable(QStringLiteral("whitePixels=%1").arg(whitePixels)));
    QVERIFY(blackPixels > 0);
  }

  void paintsBoxRenderStateRgbaOutlineLayersInLivePreview() {
    OutlinedTextItem item;
    item.setWidth(260);
    item.setHeight(110);
    item.setText(QStringLiteral("Stack"));
    item.setPixelSize(64);
    item.setColor(Qt::black);
    item.setOutlineColor(Qt::red);
    item.setOutlineLayers({QVariantMap{{QStringLiteral("enabled"), true},
                                       {QStringLiteral("color"), QStringLiteral("ff0000ff")},
                                       {QStringLiteral("size"), 2}},
                           QVariantMap{{QStringLiteral("enabled"), true},
                                       {QStringLiteral("color"), QStringLiteral("ffffffff")},
                                       {QStringLiteral("size"), 2}}});

    QImage image(260, 110, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    item.paint(&painter);

    int redPixels = 0;
    int whitePixels = 0;
    int blackPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
        const QColor c = image.pixelColor(x, y);
        if (c.red() > 120 && c.green() < 80 && c.blue() < 80 && c.alpha() > 0)
          ++redPixels;
        if (c.red() > 180 && c.green() > 180 && c.blue() > 180 && c.alpha() > 0)
          ++whitePixels;
        if (c.red() < 40 && c.green() < 40 && c.blue() < 40 && c.alpha() > 0)
          ++blackPixels;
      }
    }

    QVERIFY2(redPixels > 0,
             qPrintable(QStringLiteral("redPixels=%1 whitePixels=%2")
                            .arg(redPixels)
                            .arg(whitePixels)));
    QVERIFY2(whitePixels > 0,
             qPrintable(QStringLiteral("redPixels=%1 whitePixels=%2")
                            .arg(redPixels)
                            .arg(whitePixels)));
    QVERIFY(blackPixels > 0);
  }

  void multiOutlineMetricsUseOutermostCumulativeStrokeWidth() {
    OutlinedTextItem equalBands;
    equalBands.setOutlineLayers({QVariantMap{{QStringLiteral("enabled"), true},
                                             {QStringLiteral("color"), QStringLiteral("#ffffff")},
                                             {QStringLiteral("size"), 7}},
                                 QVariantMap{{QStringLiteral("enabled"), true},
                                             {QStringLiteral("color"), QStringLiteral("#ff0000")},
                                             {QStringLiteral("size"), 7}}});
    QCOMPARE(equalBands.effectiveMaxOutlineSizeForTesting(), 28.0);

    auto layers = QVariantList{
        QVariantMap{{QStringLiteral("enabled"), true},
                    {QStringLiteral("color"), QStringLiteral("#ffffff")},
                    {QStringLiteral("size"), 9}},
        QVariantMap{{QStringLiteral("enabled"), true},
                    {QStringLiteral("color"), QStringLiteral("#ff0000")},
                    {QStringLiteral("size"), 11}}};

    OutlinedTextItem metricsItem;
    metricsItem.setWidth(120);
    metricsItem.setHeight(70);
    metricsItem.setText(QStringLiteral("Italic fj"));
    metricsItem.setPixelSize(42);
    metricsItem.setItalic(true);
    metricsItem.setOutlineLayers(layers);

    OutlinedTextItem legacyMetricsItem;
    legacyMetricsItem.setWidth(120);
    legacyMetricsItem.setHeight(70);
    legacyMetricsItem.setText(QStringLiteral("Italic fj"));
    legacyMetricsItem.setPixelSize(42);
    legacyMetricsItem.setItalic(true);
    legacyMetricsItem.setOutlineSize(40);

    QCOMPARE(metricsItem.effectiveMaxOutlineSizeForTesting(), 40.0);
    QCOMPARE(metricsItem.editLayoutLeftPadding(), 20.0);
    QCOMPARE(metricsItem.editLayoutRightPadding(), 20.0);
    QVERIFY2(metricsItem.editLayoutPaintOffsetX() > 0.1 ||
                 metricsItem.editLayoutPaintOffsetY() > 0.1,
             qPrintable(QStringLiteral("offset=%1,%2")
                            .arg(metricsItem.editLayoutPaintOffsetX())
                            .arg(metricsItem.editLayoutPaintOffsetY())));
    QCOMPARE(metricsItem.editLayoutPaintOffsetX(),
             legacyMetricsItem.editLayoutPaintOffsetX());
    QCOMPARE(metricsItem.editLayoutPaintOffsetY(),
             legacyMetricsItem.editLayoutPaintOffsetY());

    OutlinedTextItem smallOnly;
    smallOnly.setWidth(160);
    smallOnly.setHeight(82);
    smallOnly.setText(QStringLiteral("Alpha Beta Gamma"));
    smallOnly.setPixelSize(20);
    smallOnly.setOutlineLayers({layers.first()});

    OutlinedTextItem largeLater;
    largeLater.setWidth(160);
    largeLater.setHeight(82);
    largeLater.setText(QStringLiteral("Alpha Beta Gamma"));
    largeLater.setPixelSize(20);
    largeLater.setOutlineLayers(layers);

    OutlinedTextItem legacyLarge;
    legacyLarge.setWidth(160);
    legacyLarge.setHeight(82);
    legacyLarge.setText(QStringLiteral("Alpha Beta Gamma"));
    legacyLarge.setPixelSize(20);
    legacyLarge.setOutlineSize(40);

    QCOMPARE(largeLater.wrappedLinesForTesting(),
             legacyLarge.wrappedLinesForTesting());
    QVERIFY2(largeLater.wrappedLinesForTesting().size() >=
                 smallOnly.wrappedLinesForTesting().size(),
             qPrintable(QStringLiteral("small=%1 large=%2")
                            .arg(smallOnly.wrappedLinesForTesting().join('|'))
                            .arg(largeLater.wrappedLinesForTesting().join('|'))));
    QVERIFY(largeLater.effectiveMaxOutlineSizeForTesting() >
            smallOnly.effectiveMaxOutlineSizeForTesting());
  }

  void paintsReversedSizeLayersByUiOrder() {
    auto render = [](const QVariantList &layers) {
      OutlinedTextItem item;
      item.setWidth(220);
      item.setHeight(90);
      item.setText(QStringLiteral("Stack"));
      item.setPixelSize(46);
      item.setColor(Qt::black);
      item.setOutlineLayers(layers);

      QImage image(220, 90, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };
    auto countRed = [](const QImage &image) {
      int count = 0;
      for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
          const QColor c = image.pixelColor(x, y);
          if (c.red() > 120 && c.green() < 80 && c.blue() < 80 && c.alpha() > 0)
            ++count;
        }
      }
      return count;
    };

    const QVariantMap largeRed{{QStringLiteral("enabled"), true},
                               {QStringLiteral("color"), QStringLiteral("#ff0000")},
                               {QStringLiteral("size"), 11}};
    const QVariantMap smallWhite{{QStringLiteral("enabled"), true},
                                 {QStringLiteral("color"), QStringLiteral("#ffffff")},
                                 {QStringLiteral("size"), 9}};
    const int innerLargeRedPixels = countRed(render({largeRed, smallWhite}));
    const int outerLargeRedPixels = countRed(render({smallWhite, largeRed}));

    QVERIFY2(innerLargeRedPixels > outerLargeRedPixels + 1000,
             qPrintable(QStringLiteral("inner=%1 outer=%2")
                            .arg(innerLargeRedPixels)
                            .arg(outerLargeRedPixels)));
  }

  void centersTextVertically() {
    const QColor background(Qt::transparent);
    OutlinedTextItem item;
    item.setWidth(180);
    item.setHeight(100);
    item.setText(QStringLiteral("Center"));
    item.setPixelSize(32);
    item.setColor(Qt::black);

    QImage image(180, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(background);
    QPainter painter(&image);
    item.paint(&painter);

    const QRect bounds = visibleBounds(image, background);
    QVERIFY(!bounds.isEmpty());
    QVERIFY2(std::abs(bounds.center().y() - image.rect().center().y()) <= 4,
             qPrintable(QStringLiteral("bounds=%1,%2 %3x%4")
                            .arg(bounds.x())
                            .arg(bounds.y())
                            .arg(bounds.width())
                            .arg(bounds.height())));
  }

  void paintsHorizontalAlignmentToVisibleGlyphBounds() {
    const QColor background(Qt::transparent);
    auto render = [&](int alignment) {
      OutlinedTextItem item;
      item.setWidth(260);
      item.setHeight(90);
      item.setText(QStringLiteral("Short"));
      item.setPixelSize(34);
      item.setColor(Qt::black);
      item.setHorizontalAlignment(alignment);

      QImage image(260, 90, QImage::Format_ARGB32_Premultiplied);
      image.fill(background);
      QPainter painter(&image);
      item.paint(&painter);
      return visibleBounds(image, background);
    };

    const QRect left = render(Qt::AlignLeft);
    const QRect center = render(Qt::AlignHCenter);
    const QRect right = render(Qt::AlignRight);
    QVERIFY(!left.isEmpty());
    QVERIFY(!center.isEmpty());
    QVERIFY(!right.isEmpty());
    QVERIFY2(center.left() > left.left() + 40,
             qPrintable(QStringLiteral("left=%1 center=%2")
                            .arg(left.left())
                            .arg(center.left())));
    QVERIFY2(right.left() > center.left() + 40,
             qPrintable(QStringLiteral("center=%1 right=%2")
                            .arg(center.left())
                            .arg(right.left())));
    QVERIFY2(std::abs(center.center().x() - 130) <= 8,
             qPrintable(QStringLiteral("center=%1")
                            .arg(center.center().x())));
    QVERIFY2(right.right() <= 259 && right.right() > 230,
             qPrintable(QStringLiteral("right=%1")
                            .arg(right.right())));
  }

  void exportPaintsHorizontalAlignmentToVisibleGlyphBounds() {
    const QColor background(240, 240, 240, 255);
    auto render = [&](TextAlignment alignment, QRect *bounds) {
      QTemporaryDir dir;
      if (!dir.isValid())
        return false;
      const QString pagePath = dir.filePath(QStringLiteral("page.png"));
      const QString exportPath = dir.filePath(QStringLiteral("export.png"));
      QImage page(260, 90, QImage::Format_RGBA8888);
      page.fill(background);
      if (!page.save(pagePath, "PNG"))
        return false;

      DocumentModel document;
      TextBox box;
      box.text = "Short";
      box.bounds = {0.0, 0.0, 260.0, 90.0};
      box.style.fontSize = 34;
      box.style.textColor = "000000ff";
      box.style.alignment = alignment;
      document.addTextBox(box);

      std::string error;
      const RenderGraph graph;
      if (!graph.exportPagePng(document, pagePath.toStdString(),
                               exportPath.toStdString(), &error))
        return false;
      const QImage exported(exportPath);
      if (exported.isNull())
        return false;
      *bounds = visibleBounds(exported, background);
      return true;
    };

    QRect left;
    QRect center;
    QRect right;
    QVERIFY(render(TextAlignment::Left, &left));
    QVERIFY(render(TextAlignment::Center, &center));
    QVERIFY(render(TextAlignment::Right, &right));
    QVERIFY(!left.isEmpty());
    QVERIFY(!center.isEmpty());
    QVERIFY(!right.isEmpty());
    QVERIFY2(center.left() > left.left() + 40,
             qPrintable(QStringLiteral("left=%1 center=%2")
                            .arg(left.left())
                            .arg(center.left())));
    QVERIFY2(right.left() > center.left() + 40,
             qPrintable(QStringLiteral("center=%1 right=%2")
                            .arg(center.left())
                            .arg(right.left())));
  }

  void exposesEditLayoutMetricsForNormalText() {
    OutlinedTextItem item;
    item.setWidth(180);
    item.setHeight(100);
    item.setText(QStringLiteral("Center"));
    item.setPixelSize(32);
    item.setOutlineSize(8);

    QVERIFY(item.editLayoutMetricsValid());
    QCOMPARE(item.editLayoutLeftPadding(), 4.0);
    QCOMPARE(item.editLayoutRightPadding(), 4.0);
    QVERIFY2(item.editLayoutTopPadding() > item.editLayoutLeftPadding(),
             qPrintable(QStringLiteral("top=%1 left=%2")
                            .arg(item.editLayoutTopPadding())
                            .arg(item.editLayoutLeftPadding())));

    const qreal shortBoxTopPadding = item.editLayoutTopPadding();
    item.setHeight(160);
    QVERIFY(item.editLayoutTopPadding() > shortBoxTopPadding);
  }

  void editLayoutExposesFinalPaintTranslation() {
    const QColor background(Qt::transparent);
    OutlinedTextItem item;
    item.setWidth(120);
    item.setHeight(70);
    item.setText(QStringLiteral("Italic fj"));
    item.setPixelSize(42);
    item.setItalic(true);
    item.setOutlineSize(18);
    item.setColor(Qt::black);
    item.setOutlineColor(Qt::black);

    QVERIFY(item.editLayoutMetricsValid());
    QVERIFY2(item.editLayoutPaintOffsetX() > 0.1 ||
                 item.editLayoutPaintOffsetY() > 0.1,
             qPrintable(QStringLiteral("offset=%1,%2")
                            .arg(item.editLayoutPaintOffsetX())
                            .arg(item.editLayoutPaintOffsetY())));

    QImage image(120, 70, QImage::Format_ARGB32_Premultiplied);
    image.fill(background);
    QPainter painter(&image);
    item.paint(&painter);

    const QRect bounds = visibleBounds(image, background);
    QVERIFY(!bounds.isEmpty());
    if (item.editLayoutPaintOffsetX() > 0.1)
      QVERIFY2(bounds.left() <= 6,
               qPrintable(QStringLiteral("painted left=%1 offset=%2")
                              .arg(bounds.left())
                              .arg(item.editLayoutPaintOffsetX())));
    if (item.editLayoutPaintOffsetY() > 0.1)
      QVERIFY2(bounds.top() <= 6,
               qPrintable(QStringLiteral("painted top=%1 offset=%2")
                              .arg(bounds.top())
                              .arg(item.editLayoutPaintOffsetY())));
  }

  void editLayoutMetricsUseWordWrapAndIgnoreCurvedPathText() {
    OutlinedTextItem item;
    item.setWidth(90);
    item.setHeight(140);
    item.setText(QStringLiteral("Alpha Beta Gamma"));
    item.setPixelSize(20);

    QVERIFY(item.editLayoutMetricsValid());
    const qreal wrappedTopPadding = item.editLayoutTopPadding();

    item.setWidth(600);
    QVERIFY(item.editLayoutTopPadding() > wrappedTopPadding);

    item.setPathEnabled(true);
    item.setPathPoints(QVariantList{QVariantList{0.0, 0.8},
                                    QVariantList{0.5, 0.2},
                                    QVariantList{1.0, 0.8}});
    QVERIFY(!item.editLayoutMetricsValid());
    QCOMPARE(item.editLayoutTopPadding(), 0.0);
    QCOMPARE(item.editLayoutLeftPadding(), 0.0);
  }

  void layoutMetricsCacheReusesAndInvalidatesSafely() {
    auto assertLayoutInvalidates = [](const char *name,
                                      const std::function<void(OutlinedTextItem &)> &mutate) {
      OutlinedTextItem item;
      item.setWidth(160);
      item.setHeight(90);
      item.setText(QStringLiteral("Alpha Beta Gamma"));
      item.setPixelSize(22);
      item.setOutlineSize(8);

      const int before = item.layoutCacheRebuildCountForTesting();
      QVERIFY(before > 0);

      mutate(item);
      const int after = item.layoutCacheRebuildCountForTesting();
      QVERIFY2(after > before, name);

      (void)item.editLayoutTopPadding();
      (void)item.editLayoutPaintOffsetX();
      (void)item.wrappedLinesForTesting();
      QCOMPARE(item.layoutCacheRebuildCountForTesting(), after);
    };

    const std::vector<std::pair<const char *, std::function<void(OutlinedTextItem &)>>> cases{
        {"fontFamily", [](OutlinedTextItem &item) { item.setFontFamily(QStringLiteral("Serif")); }},
        {"pixelSize", [](OutlinedTextItem &item) { item.setPixelSize(30); }},
        {"bold", [](OutlinedTextItem &item) { item.setBold(true); }},
        {"italic", [](OutlinedTextItem &item) { item.setItalic(true); }},
        {"letterSpacing", [](OutlinedTextItem &item) { item.setLetterSpacing(3); }},
        {"lineSpacing", [](OutlinedTextItem &item) { item.setLineSpacing(6); }},
        {"renderScale", [](OutlinedTextItem &item) { item.setRenderScale(1.5); }},
        {"horizontalAlignment", [](OutlinedTextItem &item) { item.setHorizontalAlignment(Qt::AlignRight); }},
        {"outlineSize", [](OutlinedTextItem &item) { item.setOutlineSize(12); }},
        {"pathMode", [](OutlinedTextItem &item) {
           item.setPathEnabled(true);
           item.setPathPoints(QVariantList{QVariantList{0.0, 0.8},
                                           QVariantList{0.5, 0.2},
                                           QVariantList{1.0, 0.8}});
           (void)item.layoutCacheRebuildCountForTesting();
           item.setPathMode(1);
         }},
        {"height", [](OutlinedTextItem &item) { item.setHeight(130); }},
    };

    for (const auto &testCase : cases)
      assertLayoutInvalidates(testCase.first, testCase.second);

    OutlinedTextItem item;
    item.setWidth(160);
    item.setHeight(90);
    item.setText(QStringLiteral("Alpha Beta Gamma"));
    item.setPixelSize(22);
    item.setOutlineSize(8);

    const int initialRebuilds = item.layoutCacheRebuildCountForTesting();
    QVERIFY(initialRebuilds > 0);
    (void)item.editLayoutTopPadding();
    (void)item.editLayoutLeftPadding();
    (void)item.editLayoutRightPadding();
    (void)item.editLayoutPaintOffsetX();
    (void)item.editLayoutPaintOffsetY();
    (void)item.wrappedLinesForTesting();
    QCOMPARE(item.layoutCacheRebuildCountForTesting(), initialRebuilds);

    item.setText(QStringLiteral("Alpha Beta Gamma Delta"));
    const int afterText = item.layoutCacheRebuildCountForTesting();
    QVERIFY(afterText > initialRebuilds);
    (void)item.editLayoutTopPadding();
    (void)item.wrappedLinesForTesting();
    QCOMPARE(item.layoutCacheRebuildCountForTesting(), afterText);

    item.setOutlineLayers({QVariantMap{{QStringLiteral("enabled"), true},
                                       {QStringLiteral("color"), QStringLiteral("#ffffff")},
                                       {QStringLiteral("size"), 6}},
                           QVariantMap{{QStringLiteral("enabled"), true},
                                       {QStringLiteral("color"), QStringLiteral("#ff0000")},
                                       {QStringLiteral("size"), 4}}});
    const int afterOutlineLayers = item.layoutCacheRebuildCountForTesting();
    QVERIFY(afterOutlineLayers > afterText);

    item.setPathEnabled(true);
    item.setPathPoints(QVariantList{QVariantList{0.0, 0.8},
                                    QVariantList{0.5, 0.2},
                                    QVariantList{1.0, 0.8}});
    const int afterPath = item.layoutCacheRebuildCountForTesting();
    QVERIFY(afterPath > afterOutlineLayers);
    QVERIFY(!item.editLayoutMetricsValid());
    QCOMPARE(item.editLayoutTopPadding(), 0.0);
    QCOMPARE(item.layoutCacheRebuildCountForTesting(), afterPath);

    item.setWidth(220);
    const int afterGeometry = item.layoutCacheRebuildCountForTesting();
    QVERIFY(afterGeometry > afterPath);
    (void)item.editLayoutPaintOffsetX();
    QCOMPARE(item.layoutCacheRebuildCountForTesting(), afterGeometry);
  }

  void blurSoftensRenderedText() {
    auto render = [](int blurSize) {
      OutlinedTextItem item;
      item.setWidth(180);
      item.setHeight(80);
      item.setText(QStringLiteral("Blur"));
      item.setPixelSize(46);
      item.setColor(Qt::black);
      item.setBlurSize(blurSize);

      QImage image(180, 80, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };
    const QImage sharp = render(0);
    const QImage blurred = render(6);
    const int hardSharpPixels = countPixels(
        sharp, [](const QColor &color) { return color.alpha() > 220; });
    const int hardBlurredPixels = countPixels(
        blurred, [](const QColor &color) { return color.alpha() > 220; });
    const int softBlurredPixels = countPixels(blurred, [](const QColor &color) {
      return color.alpha() > 20 && color.alpha() < 220;
    });

    QVERIFY(imagesDiffer(sharp, blurred));
    QVERIFY(hardBlurredPixels < hardSharpPixels);
    QVERIFY(softBlurredPixels > 200);
  }

  void shadowBlurSoftensRenderedShadow() {
    auto render = [](int shadowBlurSize) {
      OutlinedTextItem item;
      item.setWidth(200);
      item.setHeight(90);
      item.setText(QStringLiteral("Shadow"));
      item.setPixelSize(42);
      item.setColor(Qt::transparent);
      item.setShadowEnabled(true);
      item.setShadowColor(Qt::black);
      item.setShadowOffsetX(8);
      item.setShadowOffsetY(6);
      item.setShadowBlurSize(shadowBlurSize);

      QImage image(200, 90, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };
    const QImage sharp = render(0);
    const QImage blurred = render(6);
    const int hardSharpPixels = countPixels(
        sharp, [](const QColor &color) { return color.alpha() > 220; });
    const int hardBlurredPixels = countPixels(
        blurred, [](const QColor &color) { return color.alpha() > 220; });
    const int softBlurredPixels = countPixels(blurred, [](const QColor &color) {
      return color.alpha() > 20 && color.alpha() < 220;
    });

    QVERIFY(imagesDiffer(sharp, blurred));
    QVERIFY(hardBlurredPixels < hardSharpPixels);
    QVERIFY(softBlurredPixels > 200);
  }

  void gradientAndPathAffectRenderedText() {
    auto render = [](bool gradient, bool path) {
      OutlinedTextItem item;
      item.setWidth(200);
      item.setHeight(100);
      item.setText(QStringLiteral("GradientPath"));
      item.setPixelSize(28);
      item.setColor(Qt::black);
      item.setGradientEnabled(gradient);
      item.setGradientDirection(1);
      item.setGradientColorA(Qt::red);
      item.setGradientColorB(Qt::blue);
      item.setPathEnabled(path);
      item.setPathPoints(QVariantList{QVariantList{0.0, 0.85},
                                      QVariantList{0.5, 0.2},
                                      QVariantList{1.0, 0.85}});

      QImage image(200, 100, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };

    const QImage plain = render(false, false);
    const QImage gradient = render(true, false);
    const QImage path = render(false, true);
    QVERIFY(imagesDiffer(plain, gradient));
    QVERIFY(imagesDiffer(plain, path));
  }

  void gradientPathMatchesExportSemantics() {
    const QColor background(240, 240, 240, 255);
    constexpr int width = 200;
    constexpr int height = 100;
    const QVariantList points{QVariantList{0.0, 0.85}, QVariantList{0.5, 0.2},
                              QVariantList{1.0, 0.85}};

    OutlinedTextItem item;
    item.setWidth(width);
    item.setHeight(height);
    item.setText(QStringLiteral("GradientPath"));
    item.setFontFamily(QStringLiteral("sans-serif"));
    item.setPixelSize(28);
    item.setColor(Qt::black);
    item.setGradientEnabled(true);
    item.setGradientDirection(1);
    item.setGradientColorA(Qt::red);
    item.setGradientColorB(Qt::blue);
    item.setPathEnabled(true);
    item.setPathPoints(points);

    QImage live(width, height, QImage::Format_ARGB32_Premultiplied);
    live.fill(background);
    QPainter painter(&live);
    item.paint(&painter);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString pagePath = dir.filePath(QStringLiteral("page.png"));
    const QString exportPath = dir.filePath(QStringLiteral("export.png"));
    QImage page(width, height, QImage::Format_RGBA8888);
    page.fill(background);
    QVERIFY(page.save(pagePath, "PNG"));

    DocumentModel document;
    TextBox box;
    box.text = "GradientPath";
    box.bounds = {0.0, 0.0, width, height};
    box.style.fontSize = 28;
    box.style.textColor = "000000ff";
    box.effects.gradientEnabled = true;
    box.effects.gradientDirection = 1;
    box.effects.gradientColorA = "ff0000ff";
    box.effects.gradientColorB = "0000ffff";
    box.effects.pathEnabled = true;
    box.effects.pathPoints = {{0.0, 0.85}, {0.5, 0.2}, {1.0, 0.85}};
    document.addTextBox(box);

    std::string error;
    const RenderGraph graph;
    QVERIFY2(graph.exportPagePng(document, pagePath.toStdString(),
                                 exportPath.toStdString(), &error),
             error.c_str());
    const QImage exported(exportPath);
    QVERIFY(!exported.isNull());

    const QRect liveBounds = visibleBounds(live, background);
    const QRect exportBounds = visibleBounds(exported, background);
    QVERIFY(!liveBounds.isEmpty());
    QVERIFY(!exportBounds.isEmpty());
    QVERIFY(std::abs(liveBounds.center().x() - exportBounds.center().x()) <= 8);
    QVERIFY(std::abs(liveBounds.center().y() - exportBounds.center().y()) <= 8);

    const int liveRed = countPixels(live, [&](const QColor &color) {
      return color != background && color.red() > color.blue() + 30;
    });
    const int liveBlue = countPixels(live, [&](const QColor &color) {
      return color != background && color.blue() > color.red() + 30;
    });
    const int exportRed = countPixels(exported, [&](const QColor &color) {
      return color != background && color.red() > color.blue() + 30;
    });
    const int exportBlue = countPixels(exported, [&](const QColor &color) {
      return color != background && color.blue() > color.red() + 30;
    });
    QVERIFY(liveRed > 0);
    QVERIFY(liveBlue > 0);
    QVERIFY(exportRed > 0);
    QVERIFY(exportBlue > 0);
  }

  void pathPreservesMultilineTextOnFlatPath() {
    const QColor background(Qt::transparent);
    auto render = [background]() {
      OutlinedTextItem item;
      item.setWidth(260);
      item.setHeight(120);
      item.setText(QStringLiteral("HELLO\nWEAVER!"));
      item.setPixelSize(34);
      item.setLetterSpacing(1.5);
      item.setColor(Qt::black);
      item.setPathEnabled(true);
      item.setPathPoints(
          QVariantList{QVariantList{0.0, 0.65}, QVariantList{1.0, 0.65}});

      QImage image(260, 120, QImage::Format_ARGB32_Premultiplied);
      image.fill(background);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };

    const QRect pathBounds = visibleBounds(render(), background);
    QVERIFY(!pathBounds.isEmpty());
    QVERIFY2(pathBounds.height() > 50,
             qPrintable(QStringLiteral("path bounds %1x%2")
                            .arg(pathBounds.width())
                            .arg(pathBounds.height())));
    QVERIFY2(pathBounds.width() > 120 && pathBounds.width() < 260,
             qPrintable(QStringLiteral("path bounds %1x%2")
                            .arg(pathBounds.width())
                            .arg(pathBounds.height())));
  }

  void defaultFlatPathKeepsPlainVisibleBounds() {
    const QColor background(Qt::transparent);
    auto render = [background](bool pathEnabled) {
      OutlinedTextItem item;
      item.setWidth(150);
      item.setHeight(120);
      item.setText(QStringLiteral("HELLO WEAVER!"));
      item.setPixelSize(34);
      item.setLetterSpacing(1);
      item.setColor(Qt::black);
      item.setHorizontalAlignment(Qt::AlignHCenter);
      item.setPathEnabled(pathEnabled);
      item.setPathPoints(QVariantList{QVariantList{0.0, 0.5},
                                      QVariantList{0.5, 0.5},
                                      QVariantList{1.0, 0.5}});

      QImage image(150, 120, QImage::Format_ARGB32_Premultiplied);
      image.fill(background);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };

    const QImage plain = render(false);
    const QImage path = render(true);
    const QRect plainBounds = visibleBounds(plain, background);
    const QRect pathBounds = visibleBounds(path, background);
    QVERIFY(!plainBounds.isEmpty());
    QVERIFY(!pathBounds.isEmpty());
    QVERIFY2(!imagesDiffer(plain, path),
             "default flat path must render exactly like plain text");
    QVERIFY2(std::abs(plainBounds.left() - pathBounds.left()) <= 3,
             qPrintable(QStringLiteral("plain=%1 path=%2")
                            .arg(plainBounds.left())
                            .arg(pathBounds.left())));
    QVERIFY2(std::abs(plainBounds.top() - pathBounds.top()) <= 3,
             qPrintable(QStringLiteral("plain=%1 path=%2")
                            .arg(plainBounds.top())
                            .arg(pathBounds.top())));
    QVERIFY2(std::abs(plainBounds.width() - pathBounds.width()) <= 6,
             qPrintable(QStringLiteral("plain=%1 path=%2")
                            .arg(plainBounds.width())
                            .arg(pathBounds.width())));
    QVERIFY2(std::abs(plainBounds.height() - pathBounds.height()) <= 6,
             qPrintable(QStringLiteral("plain=%1 path=%2")
                            .arg(plainBounds.height())
                            .arg(pathBounds.height())));
  }

  void pathUsesVisualWrappedLinesOnFlatPath() {
    const QColor background(Qt::transparent);
    OutlinedTextItem item;
    item.setWidth(150);
    item.setHeight(120);
    item.setText(QStringLiteral("HELLO WEAVER!"));
    item.setPixelSize(34);
    item.setLetterSpacing(1.5);
    item.setColor(Qt::black);
    item.setPathEnabled(true);
    item.setPathPoints(
        QVariantList{QVariantList{0.0, 0.65}, QVariantList{1.0, 0.65}});

    QImage image(150, 120, QImage::Format_ARGB32_Premultiplied);
    image.fill(background);
    QPainter painter(&image);
    item.paint(&painter);

    const QRect pathBounds = visibleBounds(image, background);
    QVERIFY(!pathBounds.isEmpty());
    QVERIFY2(pathBounds.height() > 50,
             qPrintable(QStringLiteral("path bounds %1x%2")
                            .arg(pathBounds.width())
                            .arg(pathBounds.height())));
    QVERIFY2(pathBounds.width() < 150,
             qPrintable(QStringLiteral("path bounds %1x%2")
                            .arg(pathBounds.width())
                            .arg(pathBounds.height())));
  }

  void pathCentersLinesLikeTypeX() {
    const QColor background(Qt::transparent);
    auto render = [background](int alignment) {
      OutlinedTextItem item;
      item.setWidth(240);
      item.setHeight(100);
      item.setText(QStringLiteral("SHORT"));
      item.setPixelSize(30);
      item.setColor(Qt::black);
      item.setHorizontalAlignment(alignment);
      item.setPathEnabled(true);
      item.setPathPoints(
          QVariantList{QVariantList{0.0, 0.60}, QVariantList{1.0, 0.60}});

      QImage image(240, 100, QImage::Format_ARGB32_Premultiplied);
      image.fill(background);
      QPainter painter(&image);
      item.paint(&painter);
      return visibleBounds(image, background);
    };

    const QRect left = render(Qt::AlignLeft);
    const QRect center = render(Qt::AlignHCenter);
    const QRect right = render(Qt::AlignRight);
    QVERIFY(!left.isEmpty());
    QVERIFY(!center.isEmpty());
    QVERIFY(!right.isEmpty());
    QVERIFY2(std::abs(left.center().x() - center.center().x()) <= 3,
             qPrintable(QStringLiteral("left=%1 center=%2")
                            .arg(left.center().x())
                            .arg(center.center().x())));
    QVERIFY2(std::abs(right.center().x() - center.center().x()) <= 3,
             qPrintable(QStringLiteral("center=%1 right=%2")
                            .arg(center.center().x())
                            .arg(right.center().x())));
  }

  void pathPointDoesNotStretchHorizontalSpacing() {
    const QColor background(Qt::transparent);
    auto render = [background](const QVariantList &points) {
      OutlinedTextItem item;
      item.setWidth(240);
      item.setHeight(120);
      item.setText(QStringLiteral("EVEN SPACING"));
      item.setPixelSize(30);
      item.setColor(Qt::black);
      item.setPathEnabled(true);
      item.setPathPoints(points);

      QImage image(240, 120, QImage::Format_ARGB32_Premultiplied);
      image.fill(background);
      QPainter painter(&image);
      item.paint(&painter);
      return visibleBounds(image, background);
    };

    const QRect flat =
        render(QVariantList{QVariantList{0.0, 0.65}, QVariantList{1.0, 0.65}});
    const QRect curved =
        render(QVariantList{QVariantList{0.0, 0.65}, QVariantList{0.1, 0.20},
                            QVariantList{1.0, 0.65}});
    QVERIFY(!flat.isEmpty());
    QVERIFY(!curved.isEmpty());
    QVERIFY2(std::abs(flat.left() - curved.left()) <= 16,
             qPrintable(QStringLiteral("flat=%1 curved=%2")
                            .arg(flat.left())
                            .arg(curved.left())));
    QVERIFY2(std::abs(flat.right() - curved.right()) <= 24,
             qPrintable(QStringLiteral("flat=%1 curved=%2")
                            .arg(flat.right())
                            .arg(curved.right())));
    QVERIFY2(std::abs(flat.width() - curved.width()) <= 32,
             qPrintable(QStringLiteral("flat=%1 curved=%2")
                            .arg(flat.width())
                            .arg(curved.width())));
  }

  void samplesStraightPathByDistance() {
    OutlinedTextItem item;
    item.setPathEnabled(true);
    item.setPathPoints(QVariantList{QVariantList{0.0, 0.8},
                                    QVariantList{0.2, 0.2},
                                    QVariantList{1.0, 0.8}});

    const QPointF sample =
        item.pathBaselinePointForTesting(100.0, 200.0, 100.0);
    QVERIFY(std::abs(sample.x() - 66.1) < 0.2);
    QVERIFY(std::abs(sample.y() - 29.8) < 0.2);
  }

  void wrapsWithinOutlineInset() {
    QFile source(QStringLiteral(TEXTFX_FIXTURE_DIR
                                "/../../src/ui/OutlinedTextItem.cpp"));
    QVERIFY(source.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString code = QString::fromUtf8(source.readAll());
    QFile layoutSource(QStringLiteral(
        TEXTFX_FIXTURE_DIR "/../../src/render/RenderTextLayout.cpp"));
    QVERIFY(layoutSource.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString layoutCode = QString::fromUtf8(layoutSource.readAll());

    QVERIFY(code.contains(QStringLiteral("gaussianBlurred")));
    QVERIFY(code.contains(QStringLiteral("blurCacheKey")));
    QVERIFY(code.contains(QStringLiteral("blurSize_")));
    QVERIFY(code.contains(QStringLiteral(
        "cache.layoutWidth = std::max<qreal>(1.0, width() / cache.scale);")));
    QVERIFY(code.contains(QStringLiteral("target.scale(scale, scale);")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        layoutCode,
        QStringLiteral("const qreal paintWidth = std::max<qreal>(1.0, "
                       "options.width - options.inset * 2.0);")));
    QVERIFY(layoutCode.contains(
        QStringLiteral("document->setTextWidth(paintWidth);")));
    QVERIFY(layoutCode.contains(
        QStringLiteral("options.height - options.inset * 2.0 - blockHeight")));
    QVERIFY(layoutCode.contains(QStringLiteral("options.inset + line.x()")));
    QVERIFY(!layoutCode.contains(
        QStringLiteral("line.setLineWidth(options.width);")));
  }

  void blurCacheInvalidatesOnFractionalResize() {
    auto configure = [](OutlinedTextItem &item, qreal width) {
      item.setWidth(width);
      item.setHeight(90.1);
      item.setText(QStringLiteral("MMMMMMMMMMMMMMMMMMMMMMMM"));
      item.setPixelSize(24);
      item.setLetterSpacing(0.37);
      item.setColor(Qt::black);
      item.setBlurSize(5);
    };
    auto render = [&](OutlinedTextItem &item, qreal width) {
      configure(item, width);
      QImage image(180, 120, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter painter(&image);
      item.paint(&painter);
      return image;
    };
    auto freshRender = [&](qreal width) {
      OutlinedTextItem item;
      return render(item, width);
    };

    qreal firstWidth = 0.0;
    qreal secondWidth = 0.0;
    QImage firstFresh;
    QImage secondFresh;
    for (int width = 48; width < 160 && firstWidth == 0.0; ++width) {
      const qreal a = width + 0.1;
      const qreal b = width + 0.9;
      QImage aImage = freshRender(a);
      QImage bImage = freshRender(b);
      if (imagesDiffer(aImage, bImage)) {
        firstWidth = a;
        secondWidth = b;
        firstFresh = aImage;
        secondFresh = bImage;
      }
    }
    QVERIFY2(firstWidth > 0.0, "test setup did not find a same-ceil fractional "
                               "width that changes rendered text");

    OutlinedTextItem reusedItem;
    const QImage firstReused = render(reusedItem, firstWidth);
    const QImage secondReused = render(reusedItem, secondWidth);

    QVERIFY(!imagesDiffer(firstReused, firstFresh));
    QVERIFY(!imagesDiffer(secondReused, secondFresh));
    QVERIFY(imagesDiffer(firstReused, secondReused));
  }

  void fitsStrokeBoundsBeforePainting() {
    QFile source(QStringLiteral(TEXTFX_FIXTURE_DIR
                                "/../../src/ui/OutlinedTextItem.cpp"));
    QVERIFY(source.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString code = QString::fromUtf8(source.readAll());

    QVERIFY(sourceContainsIgnoringWhitespace(
        code, QStringLiteral("cache.paintedBounds = cache.paintedBounds.united("
                             "stroker.createStroke(cache.path)."
                             "boundingRect());")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        code,
        QStringLiteral(
            "if (paintedBounds.left() < 0.0) dx = -paintedBounds.left();")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        code,
        QStringLiteral(
            "if (paintedBounds.top() < 0.0) dy = -paintedBounds.top();")));
    QVERIFY(code.contains(QStringLiteral("path.translate(dx, dy);")));
  }
};

QTEST_MAIN(OutlinedTextItemTests)

#include "outlined_text_item_tests.moc"
