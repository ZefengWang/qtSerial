// ============================================================
// serial-debug-qml —— QML 主界面
//
// 按原型实现四页结构（基础 / 终端 / 协议解析 / 可视化），
// 采用侧边栏导航，StackView 切换页面，和 Qt Widgets 方案对齐。
//
// 架构对齐：共享 SerialWorker（C++ 端），QML 仅做 UI 层，
// 依赖 C++ 核心逻辑（复用协议/解析/字段池）。
// ============================================================
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "pages"  // 显式导入子目录页面类型（BasicPage/TerminalPage/ProtocolPage/VizPage/SettingsPage）

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 1024
    height: 768
    minimumWidth: 800
    minimumHeight: 600
    title: "Serial Debug · QML"

    color: "#16161e"

    ColumnLayout {
        id: rootLayout
        anchors.fill: parent
        spacing: 0

        // TopBar
        Rectangle {
            id: topBar
            height: 56
            color: "#24283b"
            Layout.fillWidth: true

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                Text {
                    text: "Serial Debug"
                    color: "#7aa2f7"
                    font.pointSize: 16
                    font.bold: true
                    Layout.preferredWidth: 120
                }

                Text {
                    text: mainWindow.connected ? "● 已连接" : "○ 未连接"
                    color: mainWindow.connected ? "#9ece6a" : "#565f89"
                    font.pointSize: 12
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                }
            }
        }

        // Top horizontal tab navigation (对齐新原型：替代原左侧侧边栏)
        Rectangle {
            id: navBar
            height: 44
            color: "#1f2335"
            Layout.fillWidth: true

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                spacing: 4

                NavButton {
                    text: "基础"
                    page: 0
                    isActive: mainWindow.currentPage === 0
                    onClicked: mainWindow.currentPage = 0
                }
                NavButton {
                    text: "终端"
                    page: 1
                    isActive: mainWindow.currentPage === 1
                    onClicked: mainWindow.currentPage = 1
                }
                NavButton {
                    text: "协议解析"
                    page: 2
                    isActive: mainWindow.currentPage === 2
                    onClicked: mainWindow.currentPage = 2
                }
                NavButton {
                    text: "可视化"
                    page: 3
                    isActive: mainWindow.currentPage === 3
                    onClicked: mainWindow.currentPage = 3
                }
                NavButton {
                    text: "设置"
                    page: 4
                    isActive: mainWindow.currentPage === 4
                    onClicked: mainWindow.currentPage = 4
                }
            }
        }

        // Page stack
        StackView {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            initialItem: basicPage
        }
    }

    // Top horizontal tab button component
    component NavButton: Rectangle {
        property alias text: label.text
        property int page: 0
        property bool isActive: false

        implicitWidth: label.width + 36
        height: parent.height
        color: isActive ? "#2a2e42" : "transparent"

        Rectangle {
            width: parent.width
            height: 2
            anchors.top: parent.top
            color: parent.isActive ? "#7aa2f7" : "transparent"
        }

        Text {
            id: label
            color: parent.isActive ? "#7aa2f7" : "#c0caf5"
            anchors.centerIn: parent
            font.pointSize: 13
            font.bold: parent.isActive
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }

        signal clicked
    }

    // Pages
    BasicPage {
        id: basicPage
    }
    TerminalPage {
        id: terminalPage
    }
    ProtocolPage {
        id: protocolPage
    }
    VizPage {
        id: vizPage
    }
    SettingsPage {
        id: settingsPage
    }

    property int currentPage: 0
    property bool connected: serialWorker.isOpen

    onCurrentPageChanged: {
        switch (currentPage) {
            case 0: stack.replace(basicPage); break
            case 1: stack.replace(terminalPage); break
            case 2: stack.replace(protocolPage); break
            case 3: stack.replace(vizPage); break
            case 4: stack.replace(settingsPage); break
        }
    }

    Connections {
        target: serialWorker
        function onConnectionChanged(open) { mainWindow.connected = open }
    }
}
