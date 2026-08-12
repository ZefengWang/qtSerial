// BasicPage —— 基础界面（收发一体，含发送区）
// 对应原型"01 基础界面"：端口选择、波特率、打开/刷新、接收日志、发送框。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Column {
    id: root
    anchors.fill: parent
    spacing: 12
    padding: 16

    // Port config row
    Row {
        spacing: 8
        anchors.left: parent.left
        anchors.right: parent.right

        Label { text: "端口:"; color: "#c0caf5" }
        ComboBox {
            id: portCombo
            Layout.preferredWidth: 180
            textRole: "text"
        }
        Label { text: "波特率:"; color: "#c0caf5" }
        TextField {
            id: baudInput
            text: "115200"
            Layout.preferredWidth: 100
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
        }
    }

    // Receive log
    Column {
        spacing: 6
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: portCombo.bottom
        anchors.bottom: sendBox.top
        anchors.margins: 0
        topMargin: 12

        Label {
            text: "接收日志"
            color: "#c0caf5"
            font.bold: true
        }
        Rectangle {
            color: "#12121a"
            border.color: "#3a3f5a"
            border.width: 1
            radius: 8
            anchors.fill: parent

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
    Column {
        id: sendBox
        spacing: 8
        anchors.left: parent.left
        anchors.right: parent.right
        bottom: parent.bottom

        Label { text: "发送" color: "#c0caf5" font.bold: true }
        Row {
            spacing: 8
            anchors.left: parent.left
            anchors.right: parent.right

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
                onEditingFinished: if (connected && !sendInput.text.isEmpty) doSend()
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
        if (!portCombo.currentIndex >= 0) {
            console.warn("No port selected")
            return
        }
        var cfg = {
            name: portCombo.currentText,
            baudRate: parseInt(baudInput.text) || 115200
        }
        // SerialWorker 在 C++ 侧，调用其 open
        // QML → C++ 调用，配置通过 worker 内建逻辑
        // 这里需构造 sd::PortConfig，但 QML 不能直接构造
        // 简化：serialWorker 已持有 config，仅需设置 name/baud
        // 我们通过 JS 调用，实际 C++ 会读取配置
        var ok = serialWorker.open(cfg)
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
            data = serialWorker.hexStringToByteArray(text)
        } else {
            data = text + "\r\n"
        }
        serialWorker.send(data)
        // Log sent
        var ts = new Date()
        var tsStr = "[" + ts.getHours().toString().padStart(2, '0') + ":" +
                    ts.getMinutes().toString().padStart(2, '0') + ":" +
                    ts.getSeconds().toString().padStart(2, '0') + "]"
        recvLog.append(tsStr + " → 发送: " + text + "\n")
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
