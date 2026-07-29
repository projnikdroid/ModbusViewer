pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#0f1115"
    readonly property color surface: "#171a21"
    readonly property color surfaceRaised: "#1f232c"
    readonly property color border: "#2a2f3a"

    readonly property color accent: "#4f8cff"
    readonly property color accentMuted: "#2c3f66"

    readonly property color textPrimary: "#e8eaed"
    readonly property color textSecondary: "#9aa2b1"
    readonly property color textDisabled: "#5a6070"

    readonly property color success: "#3ecf8e"
    readonly property color warning: "#e0a63e"
    readonly property color error: "#e0554f"

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 16
    readonly property int spacingLg: 24
    readonly property int spacingXl: 40

    readonly property int radiusSm: 6
    readonly property int radiusMd: 10

    readonly property string fontFamily: "Segoe UI"
    readonly property int fontSizeSm: 12
    readonly property int fontSizeMd: 14
    readonly property int fontSizeLg: 20
    readonly property int fontSizeXl: 28
}
