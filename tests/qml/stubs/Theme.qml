pragma Singleton
import QtQuick

// Test stand-in for app/qml/theme/Theme.qml. The real one is a palette-switching
// singleton whose values don't matter to behavior tests -- only that every token
// the real controls read exists. A token the real files start using that's missing
// here shows up as a QML warning (an undefined binding), not a failure, so add it
// below if a new one appears.
QtObject {
    readonly property var accent: "#336699"
    readonly property var accentGradientStops: []
    readonly property var accentMuted: "#336699"
    readonly property var background: "#112233"
    readonly property var border: "#445566"
    readonly property var fontFamily: "Arial"
    readonly property var fontFamilyMono: "Courier New"
    readonly property var fontSizeMd: 14
    readonly property var fontSizeSm: 12
    readonly property var radiusSm: 4
    readonly property var spacingSm: 8
    readonly property var surface: "#223344"
    readonly property var surfaceOpaque: "#223344"
    readonly property var surfaceRaised: "#334455"
    readonly property var textDisabled: "#778899"
    readonly property var textPrimary: "#ffffff"
    readonly property var textSecondary: "#aabbcc"
}
