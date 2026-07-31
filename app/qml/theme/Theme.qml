pragma Singleton
import QtQuick

import ModbusViewer

// Every existing Theme.xxx binding across the app keeps reading these same
// top-level properties unchanged -- only the values behind them now depend on
// ThemeSettings.themeId. Theme.qml owns the full set of named palettes and
// their display metadata; ThemeSettings only persists an opaque id string it
// never interprets. Adding a third theme later is exactly one new entry in
// `palettes` plus one new entry in `availableThemes`, both here -- no C++
// change needed.
QtObject {
    readonly property var availableThemes: [
        { id: "glassHud", label: "Glass HUD" },
        { id: "signalConsole", label: "Signal Console" }
    ]

    readonly property var palettes: ({
        glassHud: {
            background: "#070a14",
            surface: Qt.rgba(1, 1, 1, 0.055),
            surfaceRaised: Qt.rgba(1, 1, 1, 0.09),
            // Fully opaque, unlike `surface` above -- for floating overlays
            // (ComboBox popups, standalone Popups) that render in a separate
            // layer with no guaranteed dark backdrop behind them, so the
            // "glass" translucency used on in-page panels would otherwise
            // wash out to near-invisible text-on-white.
            surfaceOpaque: "#141a2e",
            border: Qt.rgba(129 / 255, 140 / 255, 248 / 255, 0.35),

            accent: "#5eead4",
            accentMuted: "#1f3733",
            accentGradientStops: ["#5eead4", "#a5b4fc"],

            textPrimary: "#eef1fb",
            textSecondary: "#8b93b8",
            textDisabled: "#565d78",

            success: "#34d399",
            warning: "#fbbf24",
            error: "#fb7185",

            spacingXs: 4, spacingSm: 8, spacingMd: 16, spacingLg: 24, spacingXl: 40,
            radiusSm: 10, radiusMd: 18,

            fontFamily: "Segoe UI",
            fontFamilyMono: "Consolas",
            fontSizeSm: 12, fontSizeMd: 14, fontSizeLg: 20, fontSizeXl: 28,

            blurEnabled: true,
            gridBackgroundEnabled: false
        },
        signalConsole: {
            background: "#000000",
            surface: "#0a0c0a",
            surfaceRaised: "#101210",
            surfaceOpaque: "#0a0c0a",
            border: Qt.rgba(1, 176 / 255, 0, 0.16),

            accent: "#ffb000",
            accentMuted: "#3d2c0a",
            accentGradientStops: [],

            textPrimary: "#e4e2da",
            textSecondary: "#6f7368",
            textDisabled: "#454742",

            success: "#4ade80",
            warning: "#ffb000",
            error: "#ff5f5f",

            spacingXs: 4, spacingSm: 8, spacingMd: 16, spacingLg: 24, spacingXl: 40,
            radiusSm: 2, radiusMd: 2,

            fontFamily: "Consolas",
            fontFamilyMono: "Consolas",
            fontSizeSm: 12, fontSizeMd: 14, fontSizeLg: 20, fontSizeXl: 28,

            blurEnabled: false,
            gridBackgroundEnabled: true
        }
    })

    readonly property var active: palettes[ThemeSettings.themeId] || palettes.glassHud

    readonly property color background: active.background
    readonly property color surface: active.surface
    readonly property color surfaceRaised: active.surfaceRaised
    readonly property color surfaceOpaque: active.surfaceOpaque
    readonly property color border: active.border

    readonly property color accent: active.accent
    readonly property color accentMuted: active.accentMuted
    readonly property var accentGradientStops: active.accentGradientStops

    readonly property color textPrimary: active.textPrimary
    readonly property color textSecondary: active.textSecondary
    readonly property color textDisabled: active.textDisabled

    readonly property color success: active.success
    readonly property color warning: active.warning
    readonly property color error: active.error

    readonly property int spacingXs: active.spacingXs
    readonly property int spacingSm: active.spacingSm
    readonly property int spacingMd: active.spacingMd
    readonly property int spacingLg: active.spacingLg
    readonly property int spacingXl: active.spacingXl

    readonly property int radiusSm: active.radiusSm
    readonly property int radiusMd: active.radiusMd

    readonly property string fontFamily: active.fontFamily
    readonly property string fontFamilyMono: active.fontFamilyMono
    readonly property int fontSizeSm: active.fontSizeSm
    readonly property int fontSizeMd: active.fontSizeMd
    readonly property int fontSizeLg: active.fontSizeLg
    readonly property int fontSizeXl: active.fontSizeXl

    readonly property bool blurEnabled: active.blurEnabled
    readonly property bool gridBackgroundEnabled: active.gridBackgroundEnabled
}
