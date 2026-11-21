// stilwandler - rewrites a message in an absurdly exaggerated style, ready to paste.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("stilwandler");
    app.setApplicationVersion("0.2.0");

    // Matches the window to stilwandler.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("stilwandler");
    app.setWindowIcon(QIcon::fromTheme("stilwandler"));

    QQuickStyle::setStyle("Material");

    // German unless a language is picked in Omarchy; every qsTr() binding is
    // re-evaluated when that language changes.
    Translator translator({}, ":/i18n", nullptr, "de");
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
