#pragma once

#include <QObject>

namespace textfx {

class EditorLimits final : public QObject {
  Q_OBJECT
  Q_PROPERTY(double minimumBoxSize READ minimumBoxSize CONSTANT)
  Q_PROPERTY(int minimumFontSize READ minimumFontSize CONSTANT)
  Q_PROPERTY(int maximumFontSize READ maximumFontSize CONSTANT)
  Q_PROPERTY(int minimumTextSpacing READ minimumTextSpacing CONSTANT)
  Q_PROPERTY(int maximumTextSpacing READ maximumTextSpacing CONSTANT)
  Q_PROPERTY(int minimumEffectSize READ minimumEffectSize CONSTANT)
  Q_PROPERTY(int maximumEffectSize READ maximumEffectSize CONSTANT)
  Q_PROPERTY(int minimumShadowOffset READ minimumShadowOffset CONSTANT)
  Q_PROPERTY(int maximumShadowOffset READ maximumShadowOffset CONSTANT)

public:
  explicit EditorLimits(QObject *parent = nullptr);

  double minimumBoxSize() const;
  int minimumFontSize() const;
  int maximumFontSize() const;
  int minimumTextSpacing() const;
  int maximumTextSpacing() const;
  int minimumEffectSize() const;
  int maximumEffectSize() const;
  int minimumShadowOffset() const;
  int maximumShadowOffset() const;
};

} // namespace textfx
