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
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantList>

#include <cmath>

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
        "const qreal layoutWidth = std::max<qreal>(1.0, width() / scale);")));
    QVERIFY(code.contains(QStringLiteral("target.scale(scale, scale);")));
    QVERIFY(sourceContainsIgnoringWhitespace(
        layoutCode,
        QStringLiteral("const qreal paintWidth = std::max<qreal>(1.0, "
                       "options.width - options.inset * 2.0);")));
    QVERIFY(
        layoutCode.contains(QStringLiteral("line.setLineWidth(paintWidth);")));
    QVERIFY(layoutCode.contains(
        QStringLiteral("options.height - options.inset * 2.0 - blockHeight")));
    QVERIFY(layoutCode.contains(QStringLiteral("qreal x = options.inset;")));
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
        code, QStringLiteral("paintedBounds = "
                             "paintedBounds.united(stroker.createStroke(path)."
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
