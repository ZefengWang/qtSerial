// BasicPage —— 基础界面（收发一体，含发送区）
// 对应原型"01 基础界面"：端口选择、波特率、打开/刷新、接收日志、发送框。
// 布局用 ColumnLayout（Positioner 无法可靠承载子项的 anchors/fill*）。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ColumnLayout {
    id: root
    anchors.fill: parent
    anchors.margins: 16
    spacing: 12

    // Port config row
    RowLayout {
        spacing: 8
        Layout.fillWidth: true

        Label { text: "端口:"; color: "#c0caf5" }
        ComboBox {
            id: portCombo
            implicitWidth: 180
            textRole: "text"
        }
        Label { text: "波特率:"; color: "#c0caf5" }
        TextField {
            id: baudInput
            text: "115200"
            implicitWidth: 100
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 1200; top: 921600 }
        }
        Button {
            id: refreshBtn
            text: "刷新"
            onClicked: refreshPorts()
            background: Rectangle {
                color: refreshBtn.down ? "#3a3f5a" : "#2f3346"
                radius: 6
            }
        }
        Button {
            id: openBtn
            text: connected ? "关闭" : "打开"
            onClicked: connected ? closePort() : openPort()
            background: Rectangle {
                color: connected ? "#f7768e" : "#7aa2f7"
                radius: 6
            }
        }
        Text {
            text: statusText
            color: "#565f89"
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
        }
    }

    // Receive log
    ColumnLayout {
        spacing: 6
        Layout.fillWidth: true
        Layout.fillHeight: true

        Label {
            text: "接收日志"
            color: "#c0caf5"
            font.bold: true
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#12121a"
            border.color: "#3a3f5a"
            border.width: 1
            radius: 8

            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                TextEdit {
                    id: recvLog
                    text: ""
                    readOnly: true
                    font.family: "monospace"
                    color: "#c0caf5"
                    textDocument.selectionColor: "#7aa2f7"
                    wrapMode: TextEdit.Wrap
                }
            }

            Row {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: 4
                spacing: 8
                Button {
                    text: "清屏"
                    onClicked: recvLog.text = ""
                    background: Rectangle { color: "#2f3346"; radius: 4 }
                }
            }
        }
    }

    // Send box
    RowLayout {
        spacing: 8
        Layout.fillWidth: true

        TextField {
            id: sendInput
            placeholderText: "输入内容，回车发送"
            Layout.fillWidth: true
            background: Rectangle {
                color: "#12121a"
                border.color: "#3a3f5a"
                border.width: 1
                radius: 6
            }
            onEditingFinished: if (connected && sendInput.text.length > 0) doSend()
        }
        CheckBox {
            id: hexCheck
            text: "HEX"
            textColor: "#c0caf5"
        }
        Button {
            id: sendBtn
            text: "发送"
            onClicked: doSend()
            background: Rectangle { color: "#9ece6a"; radius: 6 }
        }
    }

    // --- State ---
    property bool connected: mainWindow.connected

    onConnectedChanged: {
        openBtn.text = connected ? "关闭" : "打开"
        if (connected) {
            statusText = "已连接: " + portCombo.currentText + " @ " + baudInput.text
        } else {
            statusText = "未连接"
        }
    }

    property string statusText: "未连接"

    // --- Actions ---
    function refreshPorts() {
        var ports = serialWorker.scanPorts()
        portCombo.clear()
        for (var i = 0; i < ports.length; i++) {
            portCombo.addItem({text: ports[i]})
        }
    }

    function openPort() {
        if (portCombo.currentIndex < 0) {
            console.warn("No port selected")
            return
        }
        var cfg = {
            name: portCombo.currentText,
            baudRate: parseInt(baudInput.text) || 115200
        }
        var ok = serialWorker.openDevice(cfg)
        if (!ok) {
            statusText = "打开失败: " + serialWorker.lastError()
        }
    }

    function closePort() {
        serialWorker.close()
    }

    function doSend() {
        var text = sendInput.text
        if (!text || !connected) return
        var data
        if (hexCheck.checked) {
            data = serialWorker.hexStringToByteArrayInstance(text)
        } else {
            data = text // sendData 按 QVariant 编码
        }
        var n = serialWorker.sendData(data)
        // Log sent
        var ts = new Date()
        var tsStr = "[" + ts.getHours().toString().padStart(2, '0') + ":" +
                    ts.getMinutes().toString().padStart(2, '0') + ":" +
                    ts.getSeconds().toString().padStart(2, '0') + "]"
        recvLog.append(tsStr + " → 发送(" + (n > 0 ? n + "B" : "失败") + "): " + text + "\n")
        sendInput.text = ""
    }

    // Handle incoming data from C++
    Connections {
        target: serialWorker
        onDataReceived: function(data) {
            var ts = new Date()
            var tsStr = "[" + ts.getHours().toString().padStart(2, '0') + ":" +
                        ts.getMinutes().toString().padStart(2, '0') + ":" +
                        ts.getSeconds().toString().padStart(2, '0') + ".xxx]"
            recvLog.append(tsStr + " " + data + "\n")
        }
    }

    Component.onCompleted: {
        refreshPorts()
    }
}