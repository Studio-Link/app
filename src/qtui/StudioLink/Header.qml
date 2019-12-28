import QtQuick 2.13

Item {
    Image {
        id: logo
        x: 20
        y: 8
        width: 400
        height: 34
        fillMode: Image.PreserveAspectFit
        source: "images/logo_standalone.svg"
        MouseArea {
            anchors.fill: parent
            onClicked: {applicationWindow.tabIndex = 0}
        }
    }
}
