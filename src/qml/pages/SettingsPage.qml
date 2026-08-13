// SettingsPage —— 设置界面
// 对应原型"06 设置"：左侧分类导航（软件/高级串口内联/插件管理）+ 右侧内容面板。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Column {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true
    spacing: 12
    padding: 16

    Label {
        text: "设置"
        color: "#c0caf5"
        font.pointSize: 18
        font.bold: true
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 12

        // ---------- 左侧分类导航 ----------
        Column {
            id: categoryNav
            width: 150
            spacing: 4

            Repeater {
                model: ["软件设置", "高级串口设置", "插件管理"]
                CategoryButton {
                    text: modelData
                    isActive: root.currentCategory === index
                    onClicked: {
                        root.currentCategory = index
                        settingsStack.currentIndex = index
                    }
                }
            }
        }

        // ---------- 右侧内容面板 ----------
        StackLayout {
            id: settingsStack
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ===== 软件设置 =====
            Rectangle {
                color: "#1a1b26"
                border.color: "#3a3f5a"
                border.width: 1
                radius: 8

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label { text: "软件设置"; color: "#7aa2f7"; font.bold: true; font.pointSize: 14 }

                    GridLayout {
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 10
                        Layout.fillWidth: true

                        Label { text: "主题"; color: "#c0caf5" }
                        ComboBox {
                            id: themeCombo
                            model: ["跟随系统", "深色 Tokyo Night", "浅色"]
                            Layout.fillWidth: true
                            implicitWidth: 220
                            background: Rectangle { color: "#12121a"; radius: 4; border.color: "#3a3f5a"; border.width: 1 }
                        }

                        Label { text: "语言"; color: "#c0caf5" }
                        ComboBox {
                            id: langCombo
                            model: ["中文", "English"]
                            Layout.fillWidth: true
                            implicitWidth: 220
                            background: Rectangle { color: "#12121a"; radius: 4; border.color: "#3a3f5a"; border.width: 1 }
                        }

                        Label { text: "启动时自动打开浏览器"; color: "#c0caf5" }
                        CheckBox {
                            id: autoBrowserBox
                            indicator: Rectangle {
                                width: 18; height: 18; radius: 3
                                color: autoBrowserBox.checked ? "#7aa2f7" : "#12121a"
                                border.color: "#7aa2f7"; border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: "✓"; color: "#1a1b26"
                                    visible: autoBrowserBox.checked
                                    font.bold: true
                                }
                            }
                        }

                        Label { text: "定时发送间隔（ms）"; color: "#c0caf5" }
                        SpinBox {
                            id: intervalSpin
                            from: 100; to: 60000; stepSize: 100; value: 1000; editable: true
                            Layout.fillWidth: true
                            implicitWidth: 220
                            background: Rectangle { color: "#12121a"; radius: 4; border.color: "#3a3f5a"; border.width: 1 }
                        }
                    }

                    Label {
                        text: "主题与语言即时生效；持久化保存在本地配置。"
                        color: "#565f89"
                        font.pointSize: 11
                    }
                }
            }

            // ===== 高级串口设置（内联） =====
            Rectangle {
                color: "#1a1b26"
                border.color: "#3a3f5a"
                border.width: 1
                radius: 8

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label { text: "高级串口设置"; color: "#7aa2f7"; font.bold: true; font.pointSize: 14 }

                    GridLayout {
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 10
                        Layout.fillWidth: true

                        Label { text: "数据位"; color: "#c0caf5" }
                        ComboBox { id: dataBitsCombo; model: [5, 6, 7, 8]; currentIndex: 3; Layout.fillWidth: true; implicitWidth: 220; background: Rectangle { color: "#12121a"; radius: 4; border.color: "#3a3f5a"; border.width: 1 } }

                        Label { text: "停止位"; color: "#c0caf5" }
                        ComboBox { id: stopBitsCombo; model: [1, 1.5, 2]; currentIndex: 0; Layout.fillWidth: true; implicitWidth: 220; background: Rectangle { color: "#12121a"; radius: 4; border.color: "#3a3f5a"; border.width: 1 } }

                        Label { text: "校验"; color: "#c0caf5" }
                        ComboBox { id: parityCombo; model: ["None", "Even", "Odd"]; currentIndex: 0; Layout.fillWidth: true; implicitWidth: 220; background: Rectangle { color: "#12121a"; radius: 4; border.color: "#3a3f5a"; border.width: 1 } }

                        Label { text: "流控"; color: "#c0caf5" }
                        ComboBox { id: flowCombo; model: ["None", "RTS-CTS", "XON-XOFF"]; currentIndex: 0; Layout.fillWidth: true; implicitWidth: 220; background: Rectangle { color: "#12121a"; radius: 4; border.color: "#3a3f5a"; border.width: 1 } }
                    }

                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignRight
                        Button {
                            id: applyBtn
                            text: "应用"
                            background: Rectangle { color: "#7aa2f7"; radius: 6 }
                            contentItem: Text { text: applyBtn.text; color: "#1a1b26"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                applyLabel.text = "已应用（数据位 " + dataBitsCombo.currentText +
                                                 " / 停止位 " + stopBitsCombo.currentText +
                                                 " / 校验 " + parityCombo.currentText +
                                                 " / 流控 " + flowCombo.currentText + "）"
                            }
                        }
                        Button {
                            id: resetBtn
                            text: "恢复默认"
                            background: Rectangle { color: "#12121a"; radius: 6; border.color: "#3a3f5a"; border.width: 1 }
                            contentItem: Text { text: resetBtn.text; color: "#c0caf5"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                dataBitsCombo.currentIndex = 3   // 8
                                stopBitsCombo.currentIndex = 0   // 1
                                parityCombo.currentIndex = 0     // None
                                flowCombo.currentIndex = 0       // None
                                applyLabel.text = "已恢复默认（数据位 8 / 停止位 1 / 校验 None / 流控 None）"
                            }
                        }
                    }

                    Label {
                        id: applyLabel
                        text: "仅对下次打开串口生效。"
                        color: "#565f89"
                        font.pointSize: 11
                    }
                }
            }

            // ===== 插件管理（规划中） =====
            Rectangle {
                color: "#1a1b26"
                border.color: "#3a3f5a"
                border.width: 1
                radius: 8

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label { text: "插件管理"; color: "#7aa2f7"; font.bold: true; font.pointSize: 14 }

                    Rectangle {
                        width: parent.width
                        height: 120
                        radius: 6
                        color: "#12121a"
                        Column {
                            anchors.centerIn: parent
                            spacing: 6
                            Label { anchors.horizontalCenter: parent.horizontalCenter; text: "插件系统规划中"; color: "#565f89"; font.pointSize: 14 }
                            Label { anchors.horizontalCenter: parent.horizontalCenter; text: "扩展方向：协议解析 / 数据可视化 / 导出"; color: "#3b4261"; font.pointSize: 11 }
                        }
                    }
                }
            }
        }
    }

    property int currentCategory: 0

    // 左侧分类按钮组件
    component CategoryButton: Rectangle {
        property alias text: catLabel.text
        property bool isActive: false
        signal clicked
        width: parent.width
        height: 36
        radius: 6
        color: isActive ? "#1f2335" : "transparent"
        border.color: isActive ? "#3a3f5a" : "transparent"
        border.width: 1
        Text {
            id: catLabel
            anchors.centerIn: parent
            color: parent.isActive ? "#7aa2f7" : "#c0caf5"
            font.pointSize: 13
            font.bold: parent.isActive
        }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
    }
}