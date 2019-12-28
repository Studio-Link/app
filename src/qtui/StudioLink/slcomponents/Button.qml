import QtQuick 2.13
import QtQuick.Controls 2.13
import QtQuick.Controls.impl 2.13
import QtQuick.Controls.Material 2.13
import QtQuick.Controls.Material.impl 2.13

/*
 * Override Material Button for less cpu load (no ripple effect)
 * qtquickcontrols2/src/imports/controls/material/Button.qml
 */

Button {
    id: control
    text: "Button"
    property bool hover: false

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: control.Material.buttonHeight

        radius: 2
        color: !control.enabled ? control.Material.buttonDisabledColor :
                                  control.highlighted ? control.Material.highlightedButtonColor : control.hover ? "#888" : control.Material.buttonColor

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            onPressed: {
                control.highlighted = true
            }

            onReleased: {
                control.highlighted = false
            }

            onEntered: {
                control.hover = true
            }

            onExited: {
                control.hover = false
            }
        }

        PaddedRectangle {
            y: parent.height - 4
            width: parent.width
            height: 4
            radius: 2
            topPadding: -2
            clip: true
            visible: control.checkable && (!control.highlighted || control.flat)
            color: control.checked && control.enabled ? control.Material.accentColor : control.Material.secondaryTextColor
        }
        layer.enabled: control.enabled && control.Material.buttonColor.a > 0
        layer.effect: ElevationEffect {
            elevation: control.Material.elevation
        }
    }

}


/*##^##
Designer {
    D{i:0;autoSize:true;height:480;width:640}
}
##^##*/
