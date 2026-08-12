// TerminalPage —— 终端交互界面
// 对应原型"02 终端交互"：交互式设备回显，命令历史。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Column {
    id: root
    anchors.fill: parent
    spacing: 12
    padding: 16

    // Status bar
    Rectangle {
        color: "#24283b"
        radius: 6
        height: 36
        Layout.fillWidth: true

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 16

            Text {
                text: "模式: 终端交互"
                color: "#c0caf5"
                font.pointSize: 12
            }
            Text {
                text: connected ? "● 已连接" : "○ 未连接"
                color: connected ? "#9ece6a" : "#565f89"
                font.pointSize: 12
            }
            Item { Layout.fillWidth: true }
            CheckBox {
                id: echoCheck
                text: "本地回显"
                textColor: "#c0caf5"
                checked: true
            }
        }
    }

    // Terminal output
    Rectangle {
        color: "#12121a"
        border.color: "#3a3f5a"
        border.width: 1
        radius: 8
        Layout.fillWidth: true
        Layout.fillHeight: true

        ScrollView {
            anchors.fill: parent
            anchors.margins: 8
            TextEdit {
                id: output
                text: ""
                readOnly: true
                font.family: "monospace"
                color: "#c0caf5"
                wrapMode: TextEdit.Wrap
            }
        }
    }

    // Command input line
    Rectangle {
        color: "#12121a"
        border.color: "#3a3f5a"
        border.width: 1
        radius: 6
        height: 48
        Layout.fillWidth: true

        Row {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            Text {
                text: ">"
                color: "#9ece6a"
                verticalAlignment: Text.AlignVCenter
                font.bold: true
            }
            TextField {
                id: cmdInput
                placeholderText: "输入命令，回车发送..."
                Layout.fillWidth: true
                background: Rectangle { color: "transparent" }
                onEditingFinished: doSend()
            }
        }
    }

    // --- State ---
    property bool connected: mainWindow.connected
    property list<string> history: []
    property int historyIndex: -1

    // --- Actions ---
    function appendOutput(text, style) {
        if (style === "prompt") {
            output.append("<span style='color:#9ece6a'>" + text + "</span> ")
        } else if (style === "output") {
            output.append("<span style='color:#c0caf5'>" + text + "</span>\n")
        } else {
            output.append(text + "\n")
        }
    }

    function doSend() {
        var cmd = cmdInput.text.trim()
        if (!cmd || !connected) {
            cmdInput.text = ""
            return
        }

        // Add to history
        if (history.length === 0 || history[history.length - 1] !== cmd) {
            history.push(cmd)
        }
        historyIndex = -1

        // Local echo
        if (echoCheck.checked) {
            appendOutput(cmd, "prompt")
        }

        // Send
        var bytes = cmd + "\r\n"
        serialWorker.send(bytes)
        cmdInput.text = ""
    }

    // Handle incoming
    Connections {
        target: serialWorker
        onDataReceived: function(data) {
            // Terminal mode: raw data echo
            appendOutput(data, "output")
        }
    }

    // Initial prompt
    Component.onCompleted: {
        appendOutput("Terminal ready. Type commands and press Enter.", "output")
    }
}
