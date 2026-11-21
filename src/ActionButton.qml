import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A button drawn by hand like the style chips; Material's highlighted Button
// shows no background with the Omarchy accent on the dark window.
AbstractButton {
    id: button
    property bool primary: false
    property string hint: ""

    focusPolicy: Qt.NoFocus
    padding: 10
    leftPadding: 16
    rightPadding: 16
    opacity: enabled ? 1 : 0.4

    contentItem: Row {
        spacing: 12
        Label {
            text: button.text
            font.pixelSize: 15
            font.bold: button.primary
            color: button.primary ? theme.accentForeground : "#e6e6e6"
        }
        Label {
            text: button.hint
            visible: button.hint !== ""
            font.pixelSize: 13
            opacity: 0.7
            anchors.verticalCenter: parent.verticalCenter
            color: button.primary ? theme.accentForeground : "#e6e6e6"
        }
    }
    background: Rectangle {
        radius: Math.min(8, theme.radius)
        color: button.primary ? (button.hovered ? Qt.lighter(theme.accent, 1.1) : theme.accent)
                              : (button.hovered ? "#2a2a30" : "#1c1c20")
        border.color: button.primary ? theme.accent : "#33333a"
    }
}
