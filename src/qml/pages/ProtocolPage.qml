// ProtocolPage —— 协议解析界面
// 对应原型"03 协议解析"：配置字段（名称/类型/长度/padding），
// 独立于可视化，产出字段池（数据源）供可视化界面读取。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Column {
    id: root
    anchors.fill: parent
    spacing: 12
    padding: 16

    Label {
        text: "协议解析（定义帧结构 → 产出字段作为数据源）"
        color: "#c0caf5"
        font.pointSize: 16
        font.bold: true
    }

    // Field list
    Row {
        spacing: 12
        anchors.left: parent.left
        anchors.right: parent.right
        Layout.fillWidth: true

        // Left: field list
        Column {
            width: parent.width * 0.6
            spacing: 8

            Row {
                spacing: 8
                Label { text: "字段列表"; color: "#c0caf5"; font.bold: true }
                Button {
                    text: "+ 添加字段"
                    onClicked: addField()
                    background: Rectangle { color: "#7aa2f7"; radius: 6 }
                }
                Button {
                    text: "- 删除选中"
                    onClicked: removeField()
                    background: Rectangle { color: "#f7768e"; radius: 6 }
                }
            }

            Rectangle {
                color: "#12121a"
                border.color: "#3a3f5a"
                border.width: 1
                radius: 8
                height: 300
                Layout.fillWidth: true

                ListView {
                    id: fieldList
                    anchors.fill: parent
                    anchors.margins: 8
                    model: fields
                    delegate: Rectangle {
                        width: parent.width
                        height: 32
                        color: model.selected ? "#2f3346" : "transparent"
                        radius: 4

                        Row {
                            anchors.fill: parent
                            anchors.margins: { left: 8, right: 8 }
                            spacing: 8
                            verticalAlignment: Row.AlignVCenter

                            Text { text: model.name; color: "#c0caf5" }
                            Text {
                                text: model.type
                                color: "#7aa2f7"
                                font.bold: true
                            }
                            Text {
                                text: model.length + "B"
                                color: "#565f89"
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "✕"
                                color: "#f7768e"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: removeField(index)
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: selectField(index)
                        }
                    }
                }
            }
        }

        // Right: field editing
        Column {
            width: parent.width * 0.4
            spacing: 8

            Label {
                text: "字段编辑" + (selectedIndex >= 0 ? " · 选中: " + fields[selectedIndex].name : "")
                color: "#c0caf5"
                font.bold: true
            }

            Grid {
                columns: 2
                spacing: 8

                Label { text: "名称"; color: "#c0caf5" }
                TextField {
                    id: nameInput
                    text: selectedIndex >= 0 ? fields[selectedIndex].name : ""
                    width: 160
                }

                Label { text: "类型"; color: "#c0caf5" }
                ComboBox {
                    id: typeCombo
                    model: ["uint8", "uint16", "uint32", "int8", "int16", "int32", "float", "double", "bool", "padding"]
                    width: 160
                }

                Label { text: "长度(字节)"; color: "#c0caf5" }
                TextField {
                    id: lengthInput
                    text: selectedIndex >= 0 ? fields[selectedIndex].length : "4"
                    width: 160
                    validator: IntValidator { bottom: 1; top: 128 }
                }

                Label { text: "字节序"; color: "#c0caf5" }
                ComboBox {
                    id: endianCombo
                    model: ["小端", "大端"]
                    width: 160
                }
            }

            Button {
                text: "应用字段"
                onClicked: applyField()
                background: Rectangle { color: "#9ece6a"; radius: 6 }
            }

            Button {
                text: "应用协议并产出字段池"
                onClicked: applySchema()
                background: Rectangle { color: "#7aa2f7"; radius: 6 }
            }
        }
    }

    // --- Data model ---
    property var fields: []
    property int selectedIndex: -1

    function addField() {
        var field = {
            name: "field" + (fields.length + 1),
            type: "float",
            length: 4,
            endian: "小端",
            selected: false
        }
        fields.push(field)
    }

    function removeField(index) {
        if (index >= 0 && index < fields.length) {
            fields.splice(index, 1)
            selectedIndex = -1
        }
    }

    function selectField(index) {
        selectedIndex = index
    }

    function applyField() {
        if (selectedIndex < 0) return
        fields[selectedIndex].name = nameInput.text
        fields[selectedIndex].type = typeCombo.currentText
        fields[selectedIndex].length = parseInt(lengthInput.text) || 4
        fields[selectedIndex].endian = endianCombo.currentText
    }

    function applySchema() {
        // 把字段构造成 ProtocolSchema 传给 SerialWorker
        var schema = {
            name: "cust",
            fields: []
        }
        for (var i = 0; i < fields.length; i++) {
            schema.fields.push(fields[i])
        }
        // SerialWorker::applyProtocolSchema 在 C++ 侧
        // QML 无法直接构造 sd::ProtocolSchema（C++ 结构体），
        // 这里通过暴露的 Q_INVOKABLE 或数据桥完成。
        // 简化：调用暴露到 QML 的 C++ 方法。
        var ok = serialWorker.applySchemaJson(JSON.stringify(schema))
        if (!ok) {
            console.warn("applySchema failed")
        }
    }
}