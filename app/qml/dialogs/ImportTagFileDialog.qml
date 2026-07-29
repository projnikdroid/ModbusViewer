import QtQuick
import QtQuick.Dialogs

import ModbusViewer

// Picks a CSV or JSON register-map file and imports it into tagDatabaseModel via
// TagDatabaseController. Which parser runs is decided by file extension.
FileDialog {
    id: root
    title: "Import Register Tag File"
    nameFilters: ["CSV files (*.csv)", "JSON files (*.json)", "All files (*)"]

    property var tagDatabaseModel: null

    onAccepted: {
        const path = selectedFile.toString()
        if (path.toLowerCase().endsWith(".json")) {
            TagDatabaseController.importJson(path, tagDatabaseModel)
        } else {
            TagDatabaseController.importCsv(path, tagDatabaseModel)
        }
    }
}
