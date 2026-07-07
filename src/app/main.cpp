#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <qqml.h>

#include "app/EditorController.h"
#include "app/qt/EditorLimits.h"
#include "app/qt/OutlinedTextItem.h"

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("TextFX"));
  QGuiApplication::setOrganizationName(QStringLiteral("TextFX"));

  textfx::EditorController editor;
  qmlRegisterType<textfx::OutlinedTextItem>("TextFX.Ui", 1, 0,
                                            "OutlinedTextItem");
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

  return app.exec();
}
