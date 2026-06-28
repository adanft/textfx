#pragma once

#include <string>
#include <vector>

namespace textfx {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

enum class TextAlignment { Left = 0, Center = 1, Right = 2, Fill = 3 };

struct TextStyle {
    std::string fontFamily = "sans-serif";
    int fontSize = 16;
    std::string textColor = "000000ff";
    int lineSpacing = 0;
    int letterSpacing = 0;
    bool bold = false;
    bool italic = false;
    bool uppercase = false;
    TextAlignment alignment = TextAlignment::Left;
};

struct TextEffects {
    bool outlineEnabled = false;
    std::string outlineColor = "ffffffff";
    int outlineSize = 2;
    bool blurEnabled = false;
    int blurSize = 0;
    bool shadowEnabled = false;
    std::string shadowColor = "000000ff";
    int shadowOffsetX = 4;
    int shadowOffsetY = 4;
    int shadowBlurSize = 0;
    bool gradientEnabled = false;
    int gradientDirection = 0;
    std::string gradientColorA = "ffffffff";
    std::string gradientColorB = "000000ff";
    bool pathEnabled = false;
    int pathMode = 0;
    std::vector<Point> pathPoints;
    std::vector<int> pathInactivePoints;
    bool perspectiveEnabled = false;
    Point perspectiveNw;
    Point perspectiveNe;
    Point perspectiveSe;
    Point perspectiveSw;
};

struct TextBox {
    std::string text;
    Rect bounds;
    double rotationDegrees = 0.0;
    TextStyle style;
    TextEffects effects;
};

struct TextPreset {
    std::string name;
    TextStyle style;
};

class DocumentModel {
public:
    const std::vector<TextBox>& textBoxes() const { return boxes_; }
    std::vector<TextBox>& textBoxes() { return boxes_; }

    const std::vector<TextPreset>& presets() const { return presets_; }
    std::vector<TextPreset>& presets() { return presets_; }

    bool dirty() const { return dirty_; }
    void setDirty(bool dirty) { dirty_ = dirty; }
    void markSaved() { dirty_ = false; }

    void addTextBox(TextBox box);
    void moveLayer(std::size_t from, std::size_t to);
    void clear();

private:
    std::vector<TextBox> boxes_;
    std::vector<TextPreset> presets_;
    bool dirty_ = false;
};

} // namespace textfx
