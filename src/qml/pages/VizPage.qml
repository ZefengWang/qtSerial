// VizPage —— 可视化配置界面
// 对应原型"04 可视化配置"：读取字段池数据源，选择互斥类型（波形/3D），
// 绑定数据源，多实例视图。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Column {
    id: root
    anchors.fill: parent
    spacing: 12
    padding: 16

    Label {
        text: "可视化配置（读协议字段 → 选类型 → 绑数据源）"
        color: "#c0caf5"
        font.pointSize: 16
        font.bold: true
    }

    Row {
        spacing: 12
        anchors.left: parent.left
        anchors.right: parent.right
        Layout.fillWidth: true

        // Left: data sources from field pool
        Column {
            width: parent.width * 0.3
            spacing: 8

            Label {
                text: "数据源（读自协议字段池）"
                color: "#c0caf5"
                font.bold: true
            }

            Rectangle {
                color: "#12121a"
                border.color: "#3a3f5a"
                border.width: 1
                radius: 8
                height: 220
                Layout.fillWidth: true

                ListView {
                    anchors.fill: parent
                    anchors.margins: 8
                    model: sources
                    delegate: Rectangle {
                        width: parent.width
                        height: 30
                        color: model.selected ? "#2f3346" : "transparent"
                        radius: 4

                        Row {
                            anchors.fill: parent
                            anchors.margins: { left: 8, right: 8 }
                            spacing: 8

                            Text {
                                text: model.name
                                color: "#c0caf5"
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: model.selected ? "☑" : "☐"; color: "#c0caf5" }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: toggleSource(index)
                        }
                    }
                }
            }

            Row {
                spacing: 8
                Label { text: "类型:"; color: "#c0caf5" }
                ComboBox {
                    id: typeCombo
                    model: ["波形", "3D姿态"]
                    width: 120
                }
            }

            Button {
                text: "+ 新建视图"
                onClicked: addView()
                background: Rectangle { color: "#7aa2f7"; radius: 6 }
            }
        }

        // Right: view canvas
        Column {
            width: parent.width * 0.7
            spacing: 8

            Label { text: "视图画布"; color: "#c0caf5"; font.bold: true }

            ListView {
                id: viewList
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                model: views
                delegate: Rectangle {
                    width: parent.width
                    height: 140
                    color: "#12121a"
                    border.color: "#3a3f5a"
                    border.width: 1
                    radius: 8

                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Row {
                            width: parent.width
                            spacing: 8
                            Text {
                                text: model.title
                                color: "#7aa2f7"
                                font.bold: true
                            }
                            Text {
                                text: model.type
                                color: "#565f89"
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "✕"
                                color: "#f7768e"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: removeView(index)
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 90
                            color: "#0d0d14"
                            radius: 6

                            // Simplified waveform / 3D placeholder
                            Text {
                                anchors.centerIn: parent
                                text: model.type === "3D姿态" ? "3D姿态视图" :
                                      (model.fields.length > 0 ? "波形: " + model.fields.join(" + ") : "波形视图")
                                color: "#565f89"
                            }
                        }
                    }
                }
            }
        }
    }

    // --- Data ---
    property var sources: []
    property var views: []

    function loadSources() {
        // 从字段池读取数据源（C++ 侧 FieldPool）
        // SerialWorker.fieldPool 暴露到 QML 后遍历
        var pool = serialWorker.fieldPool  // 若暴露为 QObject
        if (pool !== undefined && pool.fields !== undefined) {
            sources = []
            for (var i = 0; i < pool.fields.length; i++) {
                sources.push({name: pool.fields[i], selectable: true, selected: false})
            }
        }
    }

    function toggleSource(index) {
        if (index >= 0 && index < sources.length) {
            sources[index].selected = !sources[index].selected
        }
    }

    function addView() {
        var selectedFields = []
        for (var i = 0; i < sources.length; i++) {
            if (sources[i].selected) selectedFields.push(sources[i].name)
        }
        if (selectedFields.length === 0) return

        var type = typeCombo.currentText
        var view = {
            title: "视图" + (views.length + 1) + " · " + type,
            type: type,
            fields: selectedFields
        }
        views.push(view)
    }

    function removeView(index) {
        if (index >= 0 && index < views.length) {
            views.splice(index, 1)
        }
    }

    Component.onCompleted: {
        loadSources()
    }
}