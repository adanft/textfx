#include "app/EditorController.h"

#include "fonts/FontResolver.h"
#include "render/RenderGraph.h"

#include <QClipboard>
#include <QFont>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <algorithm>
#include <cctype>
#include <utility>

namespace textfx {
namespace {
QString toQString(const std::string& value) { return QString::fromStdString(value); }
std::string toStdString(const QString& value) { return value.toStdString(); }

std::string normalizeHexColor(QString color, const std::string& fallback)
{
    color = color.trimmed();
    if (color.startsWith(QLatin1Char('#'))) color.remove(0, 1);
    if (color.size() == 6) color += QStringLiteral("ff");
    if (color.size() != 8) return fallback;
    for (const auto ch : color) {
        if (!std::isxdigit(static_cast<unsigned char>(ch.toLatin1()))) return fallback;
    }
    return color.toLower().toStdString();
}

QVariantList pointsToVariantList(const std::vector<Point>& points);

QString resolvedFontFamily(const TextStyle& style)
{
    QFont font(toQString(style.fontFamily));
    font.setPixelSize(std::max(1, style.fontSize));
    font.setBold(style.bold);
    font.setItalic(style.italic);
    font.setLetterSpacing(QFont::AbsoluteSpacing, style.letterSpacing);
    return resolveFont(font).font.family();
}

QVariantMap toMap(const TextBox& box, int index)
{
    return {
        {"index", index},
        {"text", toQString(box.text)},
        {"x", box.bounds.x},
        {"y", box.bounds.y},
        {"w", box.bounds.w},
        {"h", box.bounds.h},
        {"rotation", box.rotationDegrees},
        {"fontFamily", toQString(box.style.fontFamily)},
        {"resolvedFontFamily", resolvedFontFamily(box.style)},
        {"fontSize", box.style.fontSize},
        {"color", toQString(box.style.textColor)},
        {"lineSpacing", box.style.lineSpacing},
        {"letterSpacing", box.style.letterSpacing},
        {"bold", box.style.bold},
        {"italic", box.style.italic},
        {"uppercase", box.style.uppercase},
        {"alignment", static_cast<int>(box.style.alignment)},
        {"outline", box.effects.outlineEnabled},
        {"outlineColor", toQString(box.effects.outlineColor)},
        {"outlineSize", box.effects.outlineSize},
        {"blur", box.effects.blurEnabled},
        {"blurSize", box.effects.blurSize},
        {"shadow", box.effects.shadowEnabled},
        {"shadowColor", toQString(box.effects.shadowColor)},
        {"shadowOffsetX", box.effects.shadowOffsetX},
        {"shadowOffsetY", box.effects.shadowOffsetY},
        {"shadowBlurSize", box.effects.shadowBlurSize},
        {"gradient", box.effects.gradientEnabled},
        {"gradientDirection", box.effects.gradientDirection},
        {"gradientColorA", toQString(box.effects.gradientColorA)},
        {"gradientColorB", toQString(box.effects.gradientColorB)},
        {"perspective", box.effects.perspectiveEnabled},
        {"perspectiveNw", QVariantList{box.effects.perspectiveNw.x, box.effects.perspectiveNw.y}},
        {"perspectiveNe", QVariantList{box.effects.perspectiveNe.x, box.effects.perspectiveNe.y}},
        {"perspectiveSe", QVariantList{box.effects.perspectiveSe.x, box.effects.perspectiveSe.y}},
        {"perspectiveSw", QVariantList{box.effects.perspectiveSw.x, box.effects.perspectiveSw.y}},
        {"path", box.effects.pathEnabled},
        {"pathMode", box.effects.pathMode},
        {"pathPoints", pointsToVariantList(box.effects.pathPoints)},
    };
}

double numberFromJson(const QJsonObject& object, const char* key, double fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toDouble() : fallback;
}

int intFromJson(const QJsonObject& object, const char* key, int fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toInt() : fallback;
}

bool boolFromJson(const QJsonObject& object, const char* key, bool fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : fallback;
}

std::string stringFromJson(const QJsonObject& object, const char* key, const std::string& fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isString() ? value.toString().toStdString() : fallback;
}

Point pointFromJson(const QJsonValue& value, Point fallback = {})
{
    const auto array = value.toArray();
    return array.size() >= 2 ? Point{array.at(0).toDouble(), array.at(1).toDouble()} : fallback;
}

Point clampedUnitPoint(double x, double y)
{
    return {std::clamp(x, 0.0, 1.0), std::clamp(y, 0.0, 1.0)};
}

std::vector<Point> pointsFromJson(const QJsonValue& value)
{
    std::vector<Point> points;
    for (const auto& item : value.toArray()) points.push_back(pointFromJson(item));
    return points;
}

QVariantList pointsToVariantList(const std::vector<Point>& points)
{
    QVariantList result;
    for (const auto& point : points) result.push_back(QVariantList{point.x, point.y});
    return result;
}

bool exportPage(const ProjectStore& store, const RenderGraph& graph, const std::filesystem::path& page, const DocumentModel& document, std::string* error)
{
    return graph.exportPagePng(document, page, store.pageExportPathFor(page), error);
}

TextBox boxFromClipboardJson(const QJsonObject& object)
{
    TextBox box;
    box.text = stringFromJson(object, "text", box.text);
    box.bounds.x = numberFromJson(object, "x", box.bounds.x) + 16.0;
    box.bounds.y = numberFromJson(object, "y", box.bounds.y) + 16.0;
    box.bounds.w = std::max(20.0, numberFromJson(object, "w", box.bounds.w));
    box.bounds.h = std::max(20.0, numberFromJson(object, "h", box.bounds.h));
    box.rotationDegrees = numberFromJson(object, "rotation", box.rotationDegrees);

    auto& style = box.style;
    style.fontFamily = stringFromJson(object, "fontFamily", style.fontFamily);
    style.fontSize = intFromJson(object, "fontSize", style.fontSize);
    style.textColor = stringFromJson(object, "color", style.textColor);
    style.lineSpacing = intFromJson(object, "lineSpacing", style.lineSpacing);
    style.letterSpacing = intFromJson(object, "letterSpacing", style.letterSpacing);
    style.bold = boolFromJson(object, "bold", style.bold);
    style.italic = boolFromJson(object, "italic", style.italic);
    style.uppercase = boolFromJson(object, "uppercase", style.uppercase);
    style.alignment = static_cast<TextAlignment>(intFromJson(object, "alignment", static_cast<int>(style.alignment)));

    auto& effects = box.effects;
    effects.outlineEnabled = boolFromJson(object, "outline", effects.outlineEnabled);
    effects.outlineColor = stringFromJson(object, "outlineColor", effects.outlineColor);
    effects.outlineSize = intFromJson(object, "outlineSize", effects.outlineSize);
    effects.blurEnabled = boolFromJson(object, "blur", effects.blurEnabled);
    effects.blurSize = intFromJson(object, "blurSize", effects.blurSize);
    effects.shadowEnabled = boolFromJson(object, "shadow", effects.shadowEnabled);
    effects.shadowColor = stringFromJson(object, "shadowColor", effects.shadowColor);
    effects.shadowOffsetX = intFromJson(object, "shadowOffsetX", effects.shadowOffsetX);
    effects.shadowOffsetY = intFromJson(object, "shadowOffsetY", effects.shadowOffsetY);
    effects.shadowBlurSize = intFromJson(object, "shadowBlurSize", effects.shadowBlurSize);
    effects.gradientEnabled = boolFromJson(object, "gradient", effects.gradientEnabled);
    effects.gradientDirection = intFromJson(object, "gradientDirection", effects.gradientDirection);
    effects.gradientColorA = stringFromJson(object, "gradientColorA", effects.gradientColorA);
    effects.gradientColorB = stringFromJson(object, "gradientColorB", effects.gradientColorB);
    effects.perspectiveEnabled = boolFromJson(object, "perspective", effects.perspectiveEnabled);
    effects.pathEnabled = boolFromJson(object, "path", effects.pathEnabled);
    effects.pathMode = std::clamp(intFromJson(object, "pathMode", effects.pathMode), 0, 1);
    effects.perspectiveNw = pointFromJson(object.value("perspectiveNw"), effects.perspectiveNw);
    effects.perspectiveNe = pointFromJson(object.value("perspectiveNe"), effects.perspectiveNe);
    effects.perspectiveSe = pointFromJson(object.value("perspectiveSe"), effects.perspectiveSe);
    effects.perspectiveSw = pointFromJson(object.value("perspectiveSw"), effects.perspectiveSw);
    effects.pathPoints = pointsFromJson(object.value("pathPoints"));
    return box;
}
} // namespace

EditorController::EditorController(QObject* parent) : QObject(parent) {}

QVariantList EditorController::boxes() const
{
    QVariantList list;
    int index = 0;
    for (const auto& box : document_.textBoxes()) {
        list.push_back(toMap(box, index++));
    }
    return list;
}

QVariantList EditorController::layers() const
{
    QVariantList list;
    for (int index = static_cast<int>(document_.textBoxes().size()) - 1; index >= 0; --index) {
        list.push_back(toMap(document_.textBoxes()[static_cast<std::size_t>(index)], index));
    }
    return list;
}

QString EditorController::currentPageName() const
{
    if (currentPageIndex_ < 0 || currentPageIndex_ >= pages_.size()) return {};
    return pages_.at(currentPageIndex_);
}

QUrl EditorController::currentPageUrl() const
{
    return currentPage_.empty() ? QUrl{} : QUrl::fromLocalFile(QString::fromStdString(currentPage_.string()));
}

QUrl EditorController::rawPageUrl() const
{
    if (!store_ || currentPage_.empty()) return {};
    const auto raw = store_->rawPagePathFor(currentPage_);
    return raw.empty() ? QUrl{} : QUrl::fromLocalFile(QString::fromStdString(raw.string()));
}

QVariantList EditorController::presets() const
{
    QVariantList list;
    for (const auto& preset : document_.presets()) {
        list.push_back(QVariantMap{{"name", toQString(preset.name)}, {"fontFamily", toQString(preset.style.fontFamily)}, {"fontSize", preset.style.fontSize}, {"isDefault", ProjectStore::isDefaultTextPresetName(preset.name)}});
    }
    return list;
}

QStringList EditorController::pageTexts() const
{
    QStringList list;
    if (const auto found = pageTexts_.find(currentPageKey()); found != pageTexts_.end()) {
        for (const auto& text : found->second) list.push_back(toQString(text));
    }
    return list;
}

QStringList EditorController::pageLabels() const
{
    QStringList labels;
    for (int i = 0; i < pages_.size(); ++i) labels.push_back(QStringLiteral("%1 - %2").arg(i + 1).arg(pages_.at(i)));
    return labels;
}

bool EditorController::actionEnabled(const QString& command) const
{
    if (command == QStringLiteral("new") || command == QStringLiteral("open")) {
        return true;
    }
    if (command == QStringLiteral("paste")) {
        return hasProject();
    }
    if (command == QStringLiteral("copy") || command == QStringLiteral("delete") || command == QStringLiteral("duplicate")) {
        return selectedBox() != nullptr;
    }
    return hasProject();
}

void EditorController::openProject(const QString& folder)
{
    if (!autosaveCurrent()) return;
    store_ = std::make_unique<ProjectStore>(folder.toStdString());
    std::string error;
    pageTexts_ = store_->loadPageTexts(&error);
    pageTextPositions_.clear();
    if (!error.empty()) setNotification(toQString(error));
    refreshPages();
    document_.clear();
    selectedIndex_ = -1;
    selectedPresetIndex_ = -1;
    currentPage_.clear();
    currentPageIndex_ = -1;
    if (!pagePaths_.empty()) {
        loadPageAt(0);
    } else {
        reloadPresets();
    }
    emit selectionChanged();
    emit pageTextsChanged();
    emit documentChanged();
    emit stateChanged();
    setNotification(hasProject() ? QStringLiteral("Project opened") : QStringLiteral("No project open"));
}

void EditorController::newDocument()
{
    if (!autosaveCurrent()) return;
    store_ = std::make_unique<ProjectStore>(std::filesystem::current_path());
    currentPage_.clear();
    currentPageIndex_ = -1;
    pagePaths_.clear();
    pages_.clear();
    pageTexts_.clear();
    pageTextPositions_.clear();
    document_.clear();
    projectPresets_.clear();
    previewImageUrl_ = QUrl();
    reloadPresets();
    selectedIndex_ = -1;
    selectedPresetIndex_ = document_.presets().empty() ? -1 : 0;
    emit selectionChanged();
    emit pageTextsChanged();
    emit documentChanged();
    emit stateChanged();
    setNotification(QStringLiteral("New document"));
}

void EditorController::save()
{
    if (!hasProject()) {
        setNotification(QStringLiteral("Open a project before saving"));
        return;
    }
    if (currentPage_.empty()) {
        document_.markSaved();
        emit stateChanged();
        setNotification(QStringLiteral("Document marked saved"));
        return;
    }
    std::string error;
    if (!store_->savePage(currentPage_, document_, &error)) {
        setNotification(QStringLiteral("Could not save boxes: %1").arg(toQString(error)));
        emit stateChanged();
        return;
    }

    document_.markSaved();
    const RenderGraph graph;
    const auto exportPath = store_->pageExportPathFor(currentPage_);
    if (graph.exportPagePng(document_, currentPage_, exportPath, &error)) {
        setNotification(QStringLiteral("Saved boxes and exported PNG to %1.").arg(QString::fromStdString(exportPath.string())));
    } else {
        setNotification(QStringLiteral("Saved boxes, but could not export PNG: %1").arg(toQString(error)));
    }
    emit stateChanged();
}

void EditorController::saveAll()
{
    if (!hasProject()) {
        setNotification(QStringLiteral("Open a project before saving"));
        return;
    }
    if (currentPage_.empty()) {
        save();
        return;
    }

    int exported = 0;
    int failures = 0;
    std::string error;
    if (store_->savePage(currentPage_, document_, &error)) {
        document_.markSaved();
    } else {
        setNotification(QStringLiteral("Could not save current page; Save All stopped: %1").arg(toQString(error)));
        emit stateChanged();
        return;
    }

    const RenderGraph graph;
    for (const auto& page : pagePaths_) {
        const bool isCurrent = page == currentPage_;
        DocumentModel loaded;
        const DocumentModel* document = &document_;
        if (!isCurrent) {
            if (!store_->loadPage(page, loaded, &error)) {
                ++failures;
                continue;
            }
            document = &loaded;
        }
        if (exportPage(*store_, graph, page, *document, &error)) {
            ++exported;
        } else {
            ++failures;
        }
    }

    if (failures > 0) {
        setNotification(QStringLiteral("Exported %1 page(s), %2 failed.").arg(exported).arg(failures));
    } else {
        setNotification(QStringLiteral("Exported %1 page(s).").arg(exported));
    }
    emit stateChanged();
}

void EditorController::previousPage()
{
    if (canGoPrevious()) loadPageAt(currentPageIndex_ - 1);
}

void EditorController::nextPage()
{
    if (canGoNext()) loadPageAt(currentPageIndex_ + 1);
}

void EditorController::goToPage(int index)
{
    if (index != currentPageIndex_) loadPageAt(index);
}

void EditorController::selectBox(int index)
{
    selectedIndex_ = index >= 0 && index < static_cast<int>(document_.textBoxes().size()) ? index : -1;
    emit selectionChanged();
    emit stateChanged();
}

void EditorController::createTextBox(double x, double y, double w, double h)
{
    if (!hasProject()) {
        setNotification(QStringLiteral("Open a project before creating text"));
        return;
    }
    if (w < 12.0 || h < 12.0) return;
    TextBox box;
    box.bounds = {x, y, w, h};
    document_.addTextBox(std::move(box));
    selectedIndex_ = static_cast<int>(document_.textBoxes().size()) - 1;
    markDocumentChanged();
    emit selectionChanged();
}

void EditorController::updateSelectedText(const QString& text)
{
    if (auto* box = selectedBox()) {
        box->text = toStdString(text);
        markDocumentChanged();
    }
}

void EditorController::setSelectedFontFamily(const QString& family)
{
    if (auto* box = selectedBox()) {
        const auto value = family.trimmed();
        if (!value.isEmpty()) box->style.fontFamily = toStdString(value);
        markDocumentChanged();
    }
}

void EditorController::setSelectedFontSize(int size)
{
    if (auto* box = selectedBox()) {
        box->style.fontSize = std::clamp(size, 1, 512);
        markDocumentChanged();
    }
}

void EditorController::setSelectedTextColor(const QString& color)
{
    if (auto* box = selectedBox()) {
        box->style.textColor = normalizeHexColor(color, box->style.textColor);
        markDocumentChanged();
    }
}

void EditorController::setSelectedBold(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->style.bold = enabled;
        markDocumentChanged();
    }
}

void EditorController::setSelectedItalic(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->style.italic = enabled;
        markDocumentChanged();
    }
}

void EditorController::setSelectedUppercase(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->style.uppercase = enabled;
        markDocumentChanged();
    }
}

void EditorController::setSelectedAlignment(int alignment)
{
    if (auto* box = selectedBox()) {
        box->style.alignment = static_cast<TextAlignment>(std::clamp(alignment, 0, 2));
        markDocumentChanged();
    }
}

void EditorController::setSelectedLineSpacing(int spacing)
{
    if (auto* box = selectedBox()) {
        box->style.lineSpacing = std::clamp(spacing, -100, 300);
        markDocumentChanged();
    }
}

void EditorController::setSelectedLetterSpacing(int spacing)
{
    if (auto* box = selectedBox()) {
        box->style.letterSpacing = std::clamp(spacing, -100, 300);
        markDocumentChanged();
    }
}

void EditorController::moveSelected(double dx, double dy)
{
    if (auto* box = selectedBox()) {
        box->bounds.x += dx;
        box->bounds.y += dy;
        markDocumentChanged();
    }
}

void EditorController::resizeSelected(double dw, double dh)
{
    if (auto* box = selectedBox()) {
        box->bounds.w = std::max(12.0, box->bounds.w + dw);
        box->bounds.h = std::max(12.0, box->bounds.h + dh);
        markDocumentChanged();
    }
}

void EditorController::setSelectedBounds(double x, double y, double w, double h)
{
    if (auto* box = selectedBox()) {
        box->bounds.x = x;
        box->bounds.y = y;
        box->bounds.w = std::max(12.0, w);
        box->bounds.h = std::max(12.0, h);
        markDocumentChanged();
    }
}

void EditorController::rotateSelected(double degrees)
{
    if (auto* box = selectedBox()) {
        box->rotationDegrees += degrees;
        markDocumentChanged();
    }
}

void EditorController::setSelectedRotation(double degrees)
{
    if (auto* box = selectedBox()) {
        box->rotationDegrees = degrees;
        markDocumentChanged();
    }
}

void EditorController::setSelectedOutlineEnabled(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->effects.outlineEnabled = enabled;
        markDocumentChanged();
    }
}

void EditorController::setSelectedOutlineColor(const QString& color)
{
    if (auto* box = selectedBox()) {
        box->effects.outlineColor = normalizeHexColor(color, box->effects.outlineColor);
        markDocumentChanged();
    }
}

void EditorController::setSelectedOutlineSize(int size)
{
    if (auto* box = selectedBox()) {
        box->effects.outlineSize = std::clamp(size, 0, 128);
        markDocumentChanged();
    }
}

void EditorController::setSelectedBlurEnabled(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->effects.blurEnabled = enabled;
        markDocumentChanged();
    }
}

void EditorController::setSelectedBlurSize(int size)
{
    if (auto* box = selectedBox()) {
        box->effects.blurSize = std::clamp(size, 0, 128);
        markDocumentChanged();
    }
}

void EditorController::setSelectedShadowEnabled(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->effects.shadowEnabled = enabled;
        markDocumentChanged();
    }
}

void EditorController::setSelectedShadowColor(const QString& color)
{
    if (auto* box = selectedBox()) {
        box->effects.shadowColor = normalizeHexColor(color, box->effects.shadowColor);
        markDocumentChanged();
    }
}

void EditorController::setSelectedShadowOffsetX(int offset)
{
    if (auto* box = selectedBox()) {
        box->effects.shadowOffsetX = std::clamp(offset, -512, 512);
        markDocumentChanged();
    }
}

void EditorController::setSelectedShadowOffsetY(int offset)
{
    if (auto* box = selectedBox()) {
        box->effects.shadowOffsetY = std::clamp(offset, -512, 512);
        markDocumentChanged();
    }
}

void EditorController::setSelectedShadowBlurSize(int size)
{
    if (auto* box = selectedBox()) {
        box->effects.shadowBlurSize = std::clamp(size, 0, 128);
        markDocumentChanged();
    }
}

void EditorController::setSelectedGradientEnabled(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->effects.gradientEnabled = enabled;
        markDocumentChanged();
    }
}

void EditorController::setSelectedGradientDirection(int direction)
{
    if (auto* box = selectedBox()) {
        box->effects.gradientDirection = std::clamp(direction, 0, 1);
        markDocumentChanged();
    }
}

void EditorController::setSelectedGradientColorA(const QString& color)
{
    if (auto* box = selectedBox()) {
        box->effects.gradientColorA = normalizeHexColor(color, box->effects.gradientColorA);
        markDocumentChanged();
    }
}

void EditorController::setSelectedGradientColorB(const QString& color)
{
    if (auto* box = selectedBox()) {
        box->effects.gradientColorB = normalizeHexColor(color, box->effects.gradientColorB);
        markDocumentChanged();
    }
}

void EditorController::setSelectedPerspectiveEnabled(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->effects.perspectiveEnabled = enabled;
        markDocumentChanged();
    }
}

void EditorController::setSelectedPathEnabled(bool enabled)
{
    if (auto* box = selectedBox()) {
        box->effects.pathEnabled = enabled;
        if (enabled && box->effects.pathPoints.size() < 3) {
            box->effects.pathPoints = {{0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
        }
        markDocumentChanged();
    }
}

void EditorController::setSelectedPathMode(int mode)
{
    if (auto* box = selectedBox()) {
        box->effects.pathMode = std::clamp(mode, 0, 1);
        markDocumentChanged();
    }
}

void EditorController::resetSelectedPerspective()
{
    if (auto* box = selectedBox()) {
        box->effects.perspectiveNw = {};
        box->effects.perspectiveNe = {};
        box->effects.perspectiveSe = {};
        box->effects.perspectiveSw = {};
        markDocumentChanged();
    }
}

void EditorController::resetSelectedPath()
{
    if (auto* box = selectedBox()) {
        box->effects.pathPoints = {{0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
        markDocumentChanged();
    }
}

void EditorController::addSelectedPathPoint()
{
    if (auto* box = selectedBox()) {
        box->effects.pathEnabled = true;
        if (box->effects.pathPoints.size() < 3) box->effects.pathPoints = {{0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
        box->effects.pathPoints.insert(box->effects.pathPoints.end() - 1, {0.5, 0.5});
        markDocumentChanged();
    }
}

void EditorController::setPerspectiveHandle(const QString& corner, double x, double y)
{
    if (auto* box = selectedBox()) {
        box->effects.perspectiveEnabled = true;
        const auto point = Point{x, y};
        if (corner == QStringLiteral("ne")) box->effects.perspectiveNe = point;
        else if (corner == QStringLiteral("se")) box->effects.perspectiveSe = point;
        else if (corner == QStringLiteral("sw")) box->effects.perspectiveSw = point;
        else if (corner == QStringLiteral("n")) { box->effects.perspectiveNw.y = y; box->effects.perspectiveNe.y = y; }
        else if (corner == QStringLiteral("e")) { box->effects.perspectiveNe.x = x; box->effects.perspectiveSe.x = x; }
        else if (corner == QStringLiteral("s")) { box->effects.perspectiveSw.y = y; box->effects.perspectiveSe.y = y; }
        else if (corner == QStringLiteral("w")) { box->effects.perspectiveNw.x = x; box->effects.perspectiveSw.x = x; }
        else box->effects.perspectiveNw = point;
        markDocumentChanged();
    }
}

void EditorController::setPathHandle(int index, double x, double y)
{
    if (auto* box = selectedBox()) {
        box->effects.pathEnabled = true;
        if (box->effects.pathPoints.size() < 3) {
            box->effects.pathPoints = {{0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
        }
        if (index >= 0 && index < static_cast<int>(box->effects.pathPoints.size())) {
            box->effects.pathPoints[static_cast<std::size_t>(index)] = clampedUnitPoint(x, y);
            markDocumentChanged();
        }
    }
}

void EditorController::duplicateSelected()
{
    if (const auto* box = selectedBox()) {
        auto copy = *box;
        copy.bounds.x += 16.0;
        copy.bounds.y += 16.0;
        document_.addTextBox(std::move(copy));
        selectedIndex_ = static_cast<int>(document_.textBoxes().size()) - 1;
        markDocumentChanged();
        emit selectionChanged();
    }
}

void EditorController::deleteSelected()
{
    if (!selectedBox()) {
        return;
    }
    auto& boxes = document_.textBoxes();
    boxes.erase(boxes.begin() + selectedIndex_);
    selectedIndex_ = std::min(selectedIndex_, static_cast<int>(boxes.size()) - 1);
    document_.setDirty(true);
    markDocumentChanged();
    emit selectionChanged();
}

void EditorController::moveLayer(int to)
{
    if (!selectedBox()) {
        return;
    }
    const int from = selectedIndex_;
    to = std::clamp(to, 0, static_cast<int>(document_.textBoxes().size()) - 1);
    document_.moveLayer(static_cast<std::size_t>(from), static_cast<std::size_t>(to));
    selectedIndex_ = to;
    markDocumentChanged();
    emit selectionChanged();
}

void EditorController::copySelected()
{
    if (const auto* box = selectedBox()) {
        auto map = toMap(*box, selectedIndex_);
        QGuiApplication::clipboard()->setText(QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact));
        setNotification(QStringLiteral("Copied"));
    }
}

void EditorController::pasteBox()
{
    if (!hasProject()) {
        setNotification(QStringLiteral("Open a project before pasting"));
        return;
    }
    TextBox box;
    const auto clipboardText = QGuiApplication::clipboard()->text();
    const auto json = QJsonDocument::fromJson(clipboardText.toUtf8());
    if (json.isObject() && json.object().contains("text") && json.object().contains("x") && json.object().contains("y")) {
        box = boxFromClipboardJson(json.object());
    } else {
        box.text = clipboardText.isEmpty() ? "Text" : clipboardText.toStdString();
        box.bounds = {32.0, 32.0, 220.0, 80.0};
    }
    document_.addTextBox(std::move(box));
    selectedIndex_ = static_cast<int>(document_.textBoxes().size()) - 1;
    markDocumentChanged();
    emit selectionChanged();
}

bool EditorController::applyPageText(int index)
{
    const auto key = currentPageKey();
    const auto found = pageTexts_.find(key);
    if (found == pageTexts_.end() || index < 0 || index >= static_cast<int>(found->second.size())) return false;
    if (auto* box = selectedBox()) {
        box->text = found->second[static_cast<std::size_t>(index)];
        pageTextPositions_[key] = std::min(index + 1, static_cast<int>(found->second.size()));
        markDocumentChanged();
        emit pageTextsChanged();
        return true;
    }
    setNotification(QStringLiteral("Select a box before applying page text"));
    return false;
}

bool EditorController::applyNextPageText()
{
    const auto key = currentPageKey();
    const int index = pageTextPositions_[key];
    if (!applyPageText(index)) return false;
    return true;
}

void EditorController::selectPreset(int index)
{
    selectedPresetIndex_ = index >= 0 && index < static_cast<int>(document_.presets().size()) ? index : -1;
    emit documentChanged();
}

bool EditorController::applySelectedPreset()
{
    if (!selectedBox()) {
        setNotification(QStringLiteral("Select a box before applying a preset"));
        return false;
    }
    if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(document_.presets().size())) {
        setNotification(QStringLiteral("Select a text preset first"));
        return false;
    }
    selectedBox()->style = document_.presets().at(static_cast<std::size_t>(selectedPresetIndex_)).style;
    markDocumentChanged();
    return true;
}

bool EditorController::addPreset(const QString& name)
{
    if (!selectedBox()) {
        setNotification(QStringLiteral("Select a box before saving a preset"));
        return false;
    }
    const auto cleanName = name.trimmed().toStdString();
    if (cleanName.empty()) return false;
    const TextPreset preset{cleanName, selectedBox()->style};
    auto found = std::ranges::find_if(projectPresets_, [&](const TextPreset& item) { return item.name == cleanName; });
    if (found == projectPresets_.end()) projectPresets_.push_back(preset);
    else *found = preset;
    return saveProjectPresets(cleanName);
}

bool EditorController::updateSelectedPreset()
{
    if (!selectedBox()) {
        setNotification(QStringLiteral("Select a box before updating a preset"));
        return false;
    }
    if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(document_.presets().size())) return false;
    const auto name = document_.presets().at(static_cast<std::size_t>(selectedPresetIndex_)).name;
    const TextPreset preset{name, selectedBox()->style};
    auto found = std::ranges::find_if(projectPresets_, [&](const TextPreset& item) { return item.name == name; });
    if (found == projectPresets_.end()) projectPresets_.push_back(preset);
    else *found = preset;
    return saveProjectPresets(name);
}

bool EditorController::renameSelectedPreset(const QString& name)
{
    if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(document_.presets().size())) return false;
    const auto oldName = document_.presets().at(static_cast<std::size_t>(selectedPresetIndex_)).name;
    const auto newName = name.trimmed().toStdString();
    if (newName.empty() || ProjectStore::isDefaultTextPresetName(oldName) || ProjectStore::isDefaultTextPresetName(newName)) return false;
    if (std::ranges::any_of(document_.presets(), [&](const TextPreset& item) { return item.name == newName; })) return false;
    auto found = std::ranges::find_if(projectPresets_, [&](const TextPreset& item) { return item.name == oldName; });
    if (found == projectPresets_.end()) return false;
    found->name = newName;
    return saveProjectPresets(newName);
}

bool EditorController::deleteSelectedPreset()
{
    if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(document_.presets().size())) return false;
    const auto name = document_.presets().at(static_cast<std::size_t>(selectedPresetIndex_)).name;
    if (ProjectStore::isDefaultTextPresetName(name)) return false;
    const auto oldSize = projectPresets_.size();
    std::erase_if(projectPresets_, [&](const TextPreset& item) { return item.name == name; });
    if (projectPresets_.size() == oldSize) return false;
    return saveProjectPresets();
}

void EditorController::beginInteraction()
{
    ++interactionDepth_;
}

void EditorController::endInteraction()
{
    if (interactionDepth_ <= 0) return;
    --interactionDepth_;
    if (interactionDepth_ == 0 && pendingDocumentChanged_) {
        pendingDocumentChanged_ = false;
        updatePreviewImage();
        emit documentChanged();
        emit stateChanged();
    }
}

void EditorController::beginTextEdit()
{
    if (selectedBox() && !editingText_) {
        beginInteraction();
        editingText_ = true;
        emit stateChanged();
    }
}

void EditorController::endTextEdit()
{
    const bool wasEditing = editingText_;
    editingText_ = false;
    emit stateChanged();
    if (wasEditing) endInteraction();
}

void EditorController::notify(const QString& message)
{
    setNotification(message);
}

void EditorController::setRawVisible(bool visible)
{
    if (rawVisible_ == visible) {
        return;
    }
    rawVisible_ = visible;
    emit stateChanged();
}

TextBox* EditorController::selectedBox()
{
    return const_cast<TextBox*>(std::as_const(*this).selectedBox());
}

const TextBox* EditorController::selectedBox() const
{
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(document_.textBoxes().size())) {
        return nullptr;
    }
    return &document_.textBoxes()[static_cast<std::size_t>(selectedIndex_)];
}

void EditorController::markDocumentChanged()
{
    document_.setDirty(true);
    if (interactionDepth_ > 0) {
        pendingDocumentChanged_ = true;
        emit documentChanged();
        emit stateChanged();
        return;
    }
    updatePreviewImage();
    emit documentChanged();
    emit stateChanged();
}

void EditorController::updatePreviewImage()
{
    if (!store_ || currentPage_.empty() || !documentHasRenderEffects()) {
        previewImageUrl_ = QUrl();
        return;
    }

    const auto previewPath = store_->folder() / ProjectStore::SaveFolder / ("preview-" + currentPage_.stem().string() + "-" + std::to_string(++previewRevision_) + ".png");
    std::string error;
    const RenderGraph graph;
    if (!graph.exportPagePng(document_, currentPage_, previewPath, &error)) {
        previewImageUrl_ = QUrl();
        if (!error.empty()) setNotification(QStringLiteral("Preview unavailable: %1").arg(toQString(error)));
        return;
    }
    previewImageUrl_ = QUrl::fromLocalFile(QString::fromStdString(previewPath.string()));
}

bool EditorController::documentHasRenderEffects() const
{
    return std::ranges::any_of(document_.textBoxes(), [](const TextBox& box) {
        return box.effects.outlineEnabled || box.effects.blurEnabled || box.effects.shadowEnabled || box.effects.gradientEnabled || box.effects.perspectiveEnabled || box.effects.pathEnabled;
    });
}

void EditorController::setNotification(QString message)
{
    notification_ = std::move(message);
    emit notificationChanged();
}

void EditorController::refreshPages()
{
    pages_.clear();
    pagePaths_.clear();
    if (!store_) {
        return;
    }
    for (const auto& page : store_->listPagePaths()) {
        pagePaths_.push_back(page);
        pages_.push_back(QString::fromStdString(page.filename().string()));
    }
}

bool EditorController::loadPageAt(int index)
{
    if (!store_ || index < 0 || index >= static_cast<int>(pagePaths_.size())) return false;
    if (!autosaveCurrent()) return false;

    currentPageIndex_ = index;
    currentPage_ = pagePaths_.at(static_cast<std::size_t>(index));
    document_.clear();
    projectPresets_.clear();
    selectedIndex_ = -1;
    editingText_ = false;
    pendingDocumentChanged_ = false;
    previewImageUrl_ = QUrl();
    std::string error;
    if (!store_->loadPage(currentPage_, document_, &error)) {
        setNotification(toQString(error));
        return false;
    }
    reloadPresets();
    updatePreviewImage();
    emit selectionChanged();
    emit pageTextsChanged();
    emit documentChanged();
    emit stateChanged();
    setNotification(QStringLiteral("Page %1 of %2").arg(currentPageIndex_ + 1).arg(pageCount()));
    return true;
}

bool EditorController::autosaveCurrent()
{
    if (!store_ || currentPage_.empty()) return true;
    std::string error;
    if (store_->autosave(currentPage_, document_, &error)) return true;
    setNotification(toQString(error));
    return false;
}

std::string EditorController::currentPageKey() const
{
    return currentPage_.empty() ? std::string{} : currentPage_.filename().string();
}

bool EditorController::saveProjectPresets(const std::string& preferredName)
{
    if (!store_) return false;
    std::string error;
    if (!store_->savePresets(projectPresets_, &error)) {
        setNotification(toQString(error));
        return false;
    }
    reloadPresets(preferredName);
    return true;
}

void EditorController::reloadPresets(const std::string& preferredName)
{
    std::string currentName = preferredName;
    if (currentName.empty() && selectedPresetIndex_ >= 0 && selectedPresetIndex_ < static_cast<int>(document_.presets().size())) {
        currentName = document_.presets().at(static_cast<std::size_t>(selectedPresetIndex_)).name;
    }
    document_.presets() = ProjectStore::defaultTextPresets();
    if (store_) {
        std::string error;
        if (!store_->loadPresets(document_, projectPresets_, &error)) setNotification(toQString(error));
    }
    selectedPresetIndex_ = document_.presets().empty() ? -1 : 0;
    for (int i = 0; i < static_cast<int>(document_.presets().size()); ++i) {
        if (document_.presets().at(static_cast<std::size_t>(i)).name == currentName) selectedPresetIndex_ = i;
    }
    emit documentChanged();
}

} // namespace textfx
