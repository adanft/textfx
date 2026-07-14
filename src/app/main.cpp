#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QDebug>
#include <qqml.h>

#include "app/controllers/EditorController.h"
#include "app/qt/EditorLimits.h"
#include "app/qt/OutlinedTextItem.h"
#include "app/qt/PaintLayerItem.h"

int main(int argc, char *argv[]) {
  // Keep Qt Quick's platform-default QRhi backend while making the rendering
  // policy explicit. A specific backend can still be selected with
  // QSG_RHI_BACKEND when diagnosing platform or driver issues.
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Unknown);

  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("TextFX"));
  QGuiApplication::setOrganizationName(QStringLiteral("TextFX"));

  textfx::EditorController editor;
  qmlRegisterType<textfx::OutlinedTextItem>("TextFX.Ui", 1, 0,
                                            "OutlinedTextItem");
  qmlRegisterType<textfx::PaintLayerItem>("TextFX.Ui", 1, 0,
                                          "PaintLayerItem");
  qmlRegisterSingletonType<textfx::EditorLimits>(
      "TextFX.Ui", 1, 0, "EditorLimits",
      [](QQmlEngine *, QJSEngine *) -> QObject * {
        return new textfx::EditorLimits;
      });
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("Editor"), &editor);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule(QStringLiteral("TextFX"), QStringLiteral("Main"));

  if (!engine.rootObjects().isEmpty()) {
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
      QObject::connect(window, &QQuickWindow::sceneGraphInitialized, window,
                       [window] {
                         qInfo() << "Qt Quick graphics API:"
                                 << window->rendererInterface()->graphicsApi();
                       },
                       Qt::DirectConnection);
    }
  }

  return app.exec();
}
