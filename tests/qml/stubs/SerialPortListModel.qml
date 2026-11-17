import QtQuick

// Test stand-in for the C++ SerialPortListModel. Deliberately empty (no serial
// ports): the tests here are about the baud/parity controls, not port discovery,
// and an empty list keeps the port combo's own handlers out of the way.
ListModel {
    function rescan() {}
}
