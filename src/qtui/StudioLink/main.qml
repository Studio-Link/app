import QtQuick 2.13
import QtQuick.Window 2.13
import QtQuick.Controls 2.13
import "slcomponents" as Sl

ApplicationWindow {
    visible: true
    width: 640
    height: 480
    title: qsTr("Hello World")

    Sl.Button {
        id: myButton
    }
}
