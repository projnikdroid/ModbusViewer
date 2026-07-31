#include <QGuiApplication>
#include <QIcon>
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
    // The .exe's own embedded resource icon (app.rc) covers Explorer/the
    // taskbar's default icon before the window paints, but Qt doesn't pick
    // that up automatically as the *runtime* window/taskbar icon -- this is
    // the same app.ico, embedded separately as a Qt resource (app/CMakeLists.txt).
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("ModbusViewer", "Main");

    return app.exec();
}
