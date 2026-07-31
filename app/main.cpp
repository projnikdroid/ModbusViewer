#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSettings>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("ModbusViewer");
    QCoreApplication::setApplicationName("ModbusViewer");
    // Explicit .ini format rather than the platform-native backend (Windows
    // registry / macOS plist) -- one portable, easy-to-inspect format across
    // this project's Windows+macOS target.
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ModbusViewer", "Main");

    return app.exec();
}
