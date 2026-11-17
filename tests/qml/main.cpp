#include <QtQuickTest/quicktest.h>

#include <QtGlobal>

// Runs every tst_*.qml in this directory (QUICK_TEST_SOURCE_DIR). The app's themed
// controls are skinned against QtQuick Controls' Basic style, and the native
// Windows style ignores that customization, so pin Basic here -- the tests
// would otherwise exercise different controls than the app ships. Offscreen so
// the suite runs headless without flashing windows.
int main(int argc, char **argv)
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    return quick_test_main(argc, argv, "test_qml_controls", QUICK_TEST_SOURCE_DIR);
}
