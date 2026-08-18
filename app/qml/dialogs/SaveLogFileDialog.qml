import QtQuick
import QtQuick.Dialogs

import ModbusViewer

// Picks a destination file and starts SessionLogger writing to it.
FileDialog {
    id: root
    title: "Start Session Log"
    fileMode: FileDialog.SaveFile
    defaultSuffix: "log"
    nameFilters: ["Log files (*.log *.txt)", "All files (*)"]

    property var sessionLogger: null

    // sessionLogger.startLogging() returns false for a bad path (e.g. an
    // unwritable location) -- the caller surfaces this itself since this
    // dialog has no status label of its own to write to.
    signal startFailed()

    onAccepted: {
        if (!sessionLogger.startLogging(selectedFile.toString()))
            root.startFailed()
    }
}
