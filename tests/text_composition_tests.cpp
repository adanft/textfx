#include "infrastructure/rendering/TextComposition.h"

#include <QFont>
#include <QTest>

using namespace textfx;

class TextCompositionTests final : public QObject {
  Q_OBJECT

private slots:
  void stacksOnlyEnabledPositiveOutlineLayers() {
    const QVector<TextCompositionOutlineLayer> layers{
        {true, 2.0}, {false, 8.0}, {true, 3.0}, {true, 0.0}};

    const QVector<qreal> widths = stackedOutlineStrokeWidths(layers);
    QCOMPARE(widths, QVector<qreal>({4.0, 0.0, 10.0, 0.0}));
    QCOMPARE(maximumOutlineStrokeWidth(layers), 10.0);
    QCOMPARE(outlineInset(10.0), 5.0);
    QCOMPARE(outlineInset(0.0), 0.0);
  }

  void selectsAllValidPaths() {
    const QVector<QPointF> neutral{{0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
    const QVector<QPointF> curved{{0.0, 0.8}, {0.5, 0.2}, {1.0, 0.8}};
    QVERIFY(!usesPathText(false, curved));
    QVERIFY(usesPathText(true, neutral));
    QVERIFY(usesPathText(true, curved));

    QFont font;
    font.setPixelSize(24);
    const TextLayoutOptions options{.text = QStringLiteral("Path text"),
                                    .width = 180,
                                    .height = 80};
    const QPainterPath plain = composeTextLayoutPath(
        options, font, {.enabled = true,
                        .normalizedPoints = neutral,
                        .smooth = false});
    const QPainterPath expected = textLayoutPath(options, font);
    QCOMPARE(plain.boundingRect(), expected.boundingRect());
  }

  void preparesPlainAndPathLayoutsOnce() {
    QFont font;
    font.setPixelSize(24);
    const TextLayoutOptions options{.text = QStringLiteral("Alpha Beta\nGamma"),
                                    .width = 120,
                                    .height = 100,
                                    .horizontalAlignment = Qt::AlignHCenter};
    const QVector<QPointF> curved{{0.0, 0.7}, {0.5, 0.3}, {1.0, 0.7}};

    resetTextDocumentLayoutCountForTesting();
    const PreparedTextLayout plain = prepareTextLayout(options, font);
    QCOMPARE(textDocumentLayoutCountForTesting(), 1);
    QCOMPARE(composeTextLayoutPath(plain, options, {}), plain.plainPath);
    QCOMPARE(plain.lineTexts.join(u'|'), QStringLiteral("Alpha Beta|Gamma"));
    QCOMPARE(plain.lineTops.size(), plain.lineBaselines.size());

    resetTextDocumentLayoutCountForTesting();
    const QPainterPath path = composeTextLayoutPath(
        options, font,
        {.enabled = true, .normalizedPoints = curved, .smooth = true});
    QCOMPARE(textDocumentLayoutCountForTesting(), 1);
    QVERIFY(!path.isEmpty());
  }

  void preparedGlyphOriginsAlreadyIncludeLinePlacement() {
    QFont font;
    font.setPixelSize(24);
    const auto verify = [&font](int alignment, const QString &text) {
      const PreparedTextLayout layout = prepareTextLayout(
          {.text = text, .width = 160, .height = 120,
           .horizontalAlignment = alignment},
          font);
      QCOMPARE(layout.glyphs.size(), layout.lineXs.size());
      for (qsizetype i = 0; i < layout.glyphs.size(); ++i)
        QCOMPARE(layout.glyphs.at(i).origin,
                 QPointF(layout.lineXs.at(i), layout.lineBaselines.at(i)));
    };

    verify(Qt::AlignHCenter, QStringLiteral("M"));
    verify(Qt::AlignRight, QStringLiteral("M"));
    verify(Qt::AlignHCenter, QStringLiteral("M\nM"));
  }

  void computesRoundOutlineBoundsAndConstrainedTranslation() {
    QPainterPath path;
    path.addRect(0.0, 0.0, 10.0, 10.0);
    const QRectF painted = paintedTextBounds(path, 4.0);
    QCOMPARE(painted, QRectF(-2.0, -2.0, 14.0, 14.0));

    QCOMPARE(translationToConstrainTextBounds(painted, 20.0, 20.0, false),
             QPointF(2.0, 2.0));
    QCOMPARE(translationToConstrainTextBounds(QRectF(12.0, 14.0, 6.0, 4.0),
                                              16.0, 16.0, false),
             QPointF(-2.0, -2.0));
    QCOMPARE(translationToConstrainTextBounds(QRectF(2.0, -4.0, 8.0, 12.0),
                                              20.0, 20.0, true),
             QPointF(0.0, 0.0));
  }

  void includesShadowOffsetAndSpreadInEffectBounds() {
    QPainterPath path;
    path.addRect(2.0, 3.0, 10.0, 8.0);
    const QRectF effectBounds = effectBoundsForShadow(
        QRectF(0.0, 1.0, 14.0, 12.0), path, QPointF(5.0, -2.0), 3.0);
    QCOMPARE(effectBounds, QRectF(0.0, -2.0, 20.0, 15.0));
  }
};

QTEST_MAIN(TextCompositionTests)

#include "text_composition_tests.moc"
