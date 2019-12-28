import QtQuick 2.13
import QtQuick.Window 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.12
import "slcomponents" as Sl

ApplicationWindow {
    id: applicationWindow
    visible: true
    width: 800
    height: 480
    minimumWidth: 700
    minimumHeight: 450
    title: qsTr("Studio Link")

    property int tabIndex: 0

    onTabIndexChanged: {
       slmenu.currentIndex = tabIndex
    }


    Popup {
        id: popup
        anchors.centerIn: parent
        height: parent.height/100*80
        width: parent.width/100*80
        modal: true
        focus: true
        clip: true
        opacity: 0.8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Flow {
            y: -vbar.position * height
            ComboBox {
                id: comboBox
                model: ["First", "Second", "Third"]
                displayText: "Eingang: " + currentText
                width: 200
            }

            Rectangle {
                id: rectangle
                width: 100
                height: 100

            }
        }

        ScrollBar {
            id: vbar
            hoverEnabled: true
            active: hovered || pressed
            orientation: Qt.Vertical
            size: parent.height/1000
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    Header {

    }

    StatusCallForm {
        anchors.right: parent.right
        anchors.rightMargin: 100

    }

    TabBar {
        id: slmenu
        width: 364

        height: 42
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 27
        anchors.left: parent.left
        anchors.leftMargin: 20
        currentIndex: 0

        onCurrentIndexChanged: {
            tabIndex = currentIndex
        }

        TabButton {
            id: call
            height: slmenu.height
            text: qsTr("Calls")
            /*
            icon.name: "rocket"
            icon.source: "rocket-solid.svg"
            icon.color: Material.accent
            */
        }
        TabButton {
            id: contacts
            height: slmenu.height
            text: qsTr("Contacts")
        }
        TabButton {
            id: settings
            height: slmenu.height
            text: qsTr("Settings")
        }
    }


    StackLayout {
        anchors.rightMargin: 20
        anchors.bottomMargin: 10
        anchors.topMargin: 60
        anchors.leftMargin: 20
        anchors.fill: parent
        currentIndex: tabIndex
        Item {
            id: callTab

            ProgressBar {
                id: microLevelLeft
                anchors.top: parent.top
                anchors.topMargin: 5
                value: 0.5
            }


            ProgressBar {
                id: microLevelRight
                value: 0.8
                anchors.top: microLevelLeft.top
                anchors.topMargin: 8

            }

            Label {
                id: label
                color: "#888888"
                text: qsTr("Your signal level")
                anchors.top: microLevelRight.top
                anchors.topMargin: 5

            }
            RoundButton {
                id: roundButton
                text: ""
                anchors.left: parent.left
                anchors.leftMargin: 200
                anchors.top: parent.top
                anchors.topMargin: -10
                icon.source: "images/cog-solid.svg"
                icon.color: "#ddd"
                onClicked: popup.open()
            }

            Flow {
                spacing: 5
                anchors.topMargin: 100
                anchors.fill: parent
                anchors.margins: 4

                Repeater {
                    model: 6
                    Item {
                        id: element
                        height: 100
                        width: 245
                        Rectangle{
                            height: parent.height
                            width: parent.width
                            border.width: 1
                            color: '#000'
                            opacity: 0.3
                        }
                        Text {
                            id: name
                            color: "#ffffff"
                            text: qsTr("Hello World")
                            anchors.left: parent.left
                            anchors.leftMargin: 5
                            anchors.top: parent.top
                            anchors.topMargin: 5
                        }
                        ProgressBar {
                            anchors.left: parent.left
                            anchors.leftMargin: 5
                            anchors.top: parent.top
                            anchors.topMargin: 30
                            value: 0.5
                        }


                        Sl.Button {
                            id: cancel
                            text: "Contact"
                            anchors.left: parent.left
                            anchors.leftMargin: 5
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 0
                            font.capitalization: Font.Capitalize
                            font.pointSize: 8
                        }

                        Sl.Button {
                            text: "Mute"
                            anchors.left: parent.left
                            anchors.leftMargin: cancel.width + 10
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 0
                            font.capitalization: Font.Capitalize
                            font.pointSize: 8
                        }

                        Sl.Button {
                            text: "Cancel"
                            anchors.right: parent.right
                            anchors.rightMargin: 5
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 0
                            font.capitalization: Font.Capitalize
                            font.pointSize: 8


                        }


                    }

                }

            }

        }

        Item {
            id: contactsTab

            TextField {
                id: textField
                text: qsTr("Text Field")
                anchors.centerIn: parent
            }
        }


        Item {
            id: settingsTab


        }

    }

    Label {
        id: version
        color: "#888888"
        text: qsTr("Version: v19.12.12-beta-askdfjkldsfj")
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        font.pixelSize: 14
        MouseArea {
            anchors.fill: parent
            onClicked: Qt.openUrlExternally("https://doku.studio-link.de")
            cursorShape: Qt.PointingHandCursor

        }

    }
}
