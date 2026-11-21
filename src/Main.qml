import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 780
    height: 640
    minimumWidth: 520
    minimumHeight: 460
    visible: true
    title: "Stilwandler"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool copied: false

    function convert() {
        if (backend.ready && input.text.trim() !== "") {
            output.text = ""
            backend.convert(input.text)
        }
    }
    function copy(close) {
        if (backend.busy || output.text.trim() === "")
            return
        if (backend.copy(output.text)) {
            if (close)
                Qt.quit()
            win.copied = true
            copiedTimer.restart()
        }
    }

    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter"]; context: Qt.ApplicationShortcut; onActivated: win.convert() }
    Shortcut { sequence: "Ctrl+Shift+C"; context: Qt.ApplicationShortcut; onActivated: win.copy(true) }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: help.visible ? help.close() : help.open() }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: help.visible ? help.close() : backend.busy ? backend.cancel() : Qt.quit()
    }

    // Alt+1 to Alt+9 pick a style without leaving the text.
    Repeater {
        model: Math.min(9, backend.styles.length)
        delegate: Item {
            required property int index
            Shortcut {
                sequence: "Alt+" + (index + 1)
                context: Qt.ApplicationShortcut
                onActivated: backend.selected = backend.styles[index].key
            }
        }
    }

    Shortcut { sequence: "Alt+F"; context: Qt.ApplicationShortcut; onActivated: backend.recipient = "frau" }
    Shortcut { sequence: "Alt+M"; context: Qt.ApplicationShortcut; onActivated: backend.recipient = "mann" }
    Shortcut { sequence: "Alt+O"; context: Qt.ApplicationShortcut; onActivated: backend.recipient = "offen" }

    Timer { id: copiedTimer; interval: 1500; onTriggered: win.copied = false }

    Connections {
        target: backend
        // Assigned rather than bound, so the result stays editable.
        function onOutputChanged() { output.text = backend.output }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1
                TextArea {
                    id: input
                    objectName: "input"
                    focus: true
                    wrapMode: TextEdit.Wrap
                    placeholderText: qsTr("Your text or idea")
                    font.pixelSize: 16
                }
            }

            // Whom the message goes to: decides Held or Schildmaid.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    text: qsTr("To:")
                    font.pixelSize: 15
                    opacity: 0.7
                    Layout.rightMargin: 4
                }
                Repeater {
                    model: [
                        { key: "frau", label: qsTr("Woman"), hint: "Alt+F" },
                        { key: "mann", label: qsTr("Man"), hint: "Alt+M" },
                        { key: "offen", label: qsTr("Open"), hint: "Alt+O" }
                    ]
                    delegate: AbstractButton {
                        id: recipientChip
                        required property var modelData
                        readonly property bool selected: backend.recipient === modelData.key
                        focusPolicy: Qt.NoFocus
                        padding: 6
                        leftPadding: 12
                        rightPadding: 12
                        onClicked: backend.recipient = modelData.key
                        contentItem: Label {
                            text: recipientChip.modelData.label
                            font.pixelSize: 14
                            color: recipientChip.selected ? theme.accentForeground : "#e6e6e6"
                        }
                        background: Rectangle {
                            radius: Math.min(8, theme.radius)
                            color: recipientChip.selected ? theme.accent : recipientChip.hovered ? "#2a2a30" : "#1c1c20"
                            border.color: recipientChip.selected ? theme.accent : "#33333a"
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: modelData.hint
                    }
                }
                Item { Layout.fillWidth: true }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: backend.styles
                    // Chips drawn by hand: Material's checked Button is barely
                    // distinguishable on the dark window.
                    delegate: AbstractButton {
                        id: chip
                        required property var modelData
                        required property int index
                        readonly property bool selected: backend.selected === modelData.key
                        focusPolicy: Qt.NoFocus
                        padding: 10
                        leftPadding: 14
                        rightPadding: 14
                        onClicked: backend.selected = modelData.key
                        contentItem: Label {
                            text: chip.modelData.emoji + "  " + chip.modelData.name
                            font.pixelSize: 15
                            color: chip.selected ? theme.accentForeground : "#e6e6e6"
                        }
                        background: Rectangle {
                            radius: Math.min(8, theme.radius)
                            color: chip.selected ? theme.accent : chip.hovered ? "#2a2a30" : "#1c1c20"
                            border.color: chip.selected ? theme.accent : "#33333a"
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: index < 9 ? modelData.short + "   (Alt+" + (index + 1) + ")" : modelData.short
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ActionButton {
                    text: qsTr("Convert")
                    hint: "Ctrl+Enter"
                    primary: true
                    enabled: backend.ready && !backend.busy && input.text.trim() !== ""
                    onClicked: win.convert()
                }
                BusyIndicator {
                    running: backend.busy
                    visible: backend.busy
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                }
                ActionButton {
                    text: qsTr("Cancel")
                    visible: backend.busy
                    onClicked: backend.cancel()
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("Waiting for the keyring")
                    visible: !backend.ready && backend.error === ""
                    opacity: 0.6
                }
            }

            TextArea {
                Layout.fillWidth: true
                visible: backend.notice !== ""
                text: backend.notice
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: "#ffcc80"
                background: null
                padding: 0
            }

            TextArea {
                Layout.fillWidth: true
                visible: backend.error !== ""
                text: backend.error
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: "#ff8a80"
                background: null
                padding: 0
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1
                TextArea {
                    id: output
                    objectName: "output"
                    // Editable only once the answer is complete; each streamed
                    // piece replaces the text.
                    readOnly: backend.busy
                    wrapMode: TextEdit.Wrap
                    placeholderText: qsTr("Result")
                    font.pixelSize: 16
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ActionButton {
                    text: qsTr("Copy and close")
                    hint: "Ctrl+Shift+C"
                    primary: true
                    enabled: output.text.trim() !== "" && !backend.busy
                    onClicked: win.copy(true)
                }
                ActionButton {
                    text: qsTr("Copy")
                    enabled: output.text.trim() !== "" && !backend.busy
                    onClicked: win.copy(false)
                }
                Label {
                    text: qsTr("Copied")
                    color: theme.accent
                    visible: win.copied
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("F1 shortcuts")
                    opacity: 0.5
                }
            }
        }
    }

    Popup {
        id: help
        anchors.centerIn: parent
        modal: true
        padding: 24
        background: Rectangle { color: "#1c1c20"; radius: theme.radius; border.color: theme.accent }

        ColumnLayout {
            spacing: 8
            Repeater {
                model: [
                    ["Ctrl+Enter", qsTr("Convert (again for a new variant)")],
                    ["Alt+1 … Alt+9", qsTr("Pick a style")],
                    ["Alt+F / Alt+M / Alt+O", qsTr("Message goes to a woman / a man / open")],
                    ["Ctrl+Shift+C", qsTr("Copy the result and close")],
                    ["Esc", qsTr("Cancel, or close the window")],
                    ["F1", qsTr("Show or hide this list")]
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 24
                    Label {
                        text: modelData[0]
                        font.bold: true
                        color: theme.accent
                        Layout.preferredWidth: 170
                    }
                    Label { text: modelData[1] }
                }
            }
        }
    }
}
