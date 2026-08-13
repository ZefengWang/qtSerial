// VizPage —— 可视化配置界面
// 对应原型"04 可视化配置"：读取字段池数据源，选择互斥类型（波形/3D），
// 绑定数据源，多实例视图。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ColumnLayout {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.margins: 16
    spacing: 12

    Label {
        text: "可视化配置（读协议字段 → 选类型 → 绑数据源）"
        color: "#c0caf5"
        font.pointSize: 16
        font.bold: true
    }

    RowLayout {
        spacing: 12
        Layout.fillWidth: true
        Layout.fillHeight: true

        // Left: data sources from field pool
        ColumnLayout {
            Layout.preferredWidth: root.width * 0.3
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: sourceList
                    anchors.fill: parent
                    anchors.margins: 8
                    model: root.sources
                    delegate: Rectangle {
                        width: parent.width
                        height: 30
                        color: model.selected ? "#2f3346" : "transparent"
                        radius: 4

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            Text { text: model.name; color: "#c0caf5"; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: model.selected ? "☑" : "☐"; color: "#c0caf5" }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.toggleSource(index)
                        }
                    }
                }
            }

            RowLayout {
                spacing: 8
                Label { text: "类型:"; color: "#c0caf5" }
                ComboBox {
                    id: typeCombo
                    model: ["波形", "3D姿态"]
                    Layout.fillWidth: true
                }
            }

            Button {
                text: "+ 新建视图"
                Layout.fillWidth: true
                onClicked: root.addView()
                background: Rectangle { color: "#7aa2f7"; radius: 6 }
            }
        }

        // Right: view canvas
        ColumnLayout {
            Layout.preferredWidth: root.width * 0.7
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Label { text: "视图画布"; color: "#c0caf5"; font.bold: true }

            ListView {
                id: viewList
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                model: root.views
                delegate: Rectangle {
                    width: parent.width
                    height: 140
                    color: "#12121a"
                    border.color: "#3a3f5a"
                    border.width: 1
                    radius: 8

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        RowLayout {
                            width: parent.width
                            spacing: 8
                            Text { text: model.title; color: "#7aa2f7"; font.bold: true }
                            Text { text: model.type; color: "#565f89" }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "✕"
                                color: "#f7768e"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: root.removeView(index)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
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
        // 从字段池读取数据源（C++ 侧 FieldPool，经桥接方法返回字段名列表）。
        var names = serialWorker.fieldPoolNames()
        sources = []
        for (var i = 0; i < names.length; i++) {
            sources.push({name: names[i], selected: false})
        }
    }

    function toggleSource(index) {
        if (index >= 0 && index < sources.length) {
            var copy = sources
            copy[index] = {name: sources[index].name, selected: !sources[index].selected}
            sources = copy  // 重新赋值触发 ListView 刷新
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
        var vcopy = views
        vcopy.push(view)
        views = vcopy  // 重新赋值触发 ListView 刷新
    }

    function removeView(index) {
        if (index >= 0 && index < views.length) {
            var vcopy = views.slice()
            vcopy.splice(index, 1)
            views = vcopy
        }
    }

    Component.onCompleted: {
        loadSources()
    }
}