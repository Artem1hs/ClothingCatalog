import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 980
    height: 620
    visible: true
    title: "QML Desktop Demo"
    color: "#F4F1ED"

    property color panelColor: "#FFFFFF"
    property color accentColor: "#7A4F39"
    property color textColor: "#1D1A17"
    property color mutedColor: "#756B63"

    component MetricCard: Rectangle {
        property string title: ""
        property string value: ""
        property string subtitle: ""

        radius: 18
        color: root.panelColor
        border.color: "#E3D8CF"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 8

            Text {
                text: title
                color: root.mutedColor
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Text {
                text: value
                color: root.textColor
                font.pixelSize: 30
                font.weight: Font.Bold
            }

            Text {
                text: subtitle
                color: root.mutedColor
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 28
        radius: 28
        color: "#FBF8F4"
        border.color: "#E6DCD3"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 26
            spacing: 22

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Desktop Dashboard"
                        color: root.textColor
                        font.pixelSize: 30
                        font.weight: Font.Bold
                    }

                    Text {
                        text: "Qt Quick/QML interface sample"
                        color: root.mutedColor
                        font.pixelSize: 14
                    }
                }

                Rectangle {
                    radius: 16
                    color: root.accentColor
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 44

                    Text {
                        anchors.centerIn: parent
                        text: "ONLINE"
                        color: "white"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                spacing: 16

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Catalog items"
                    value: "42"
                    subtitle: "Loaded from local image folder"
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Orders"
                    value: "18"
                    subtitle: "Local API and SQLite storage"
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Notifications"
                    value: "WS"
                    subtitle: "WebSocket status updates"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 20
                    color: root.panelColor
                    border.color: "#E3D8CF"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 14

                        Text {
                            text: "Activity"
                            color: root.textColor
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }

                        Repeater {
                            model: ["User opened catalog", "Product added to cart", "Order status changed", "Admin updated product"]

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: 12
                                color: "#F7F1EC"

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 14
                                    text: modelData
                                    color: root.textColor
                                    font.pixelSize: 14
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 330
                    Layout.fillHeight: true
                    radius: 20
                    color: root.panelColor
                    border.color: "#E3D8CF"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 16

                        Text {
                            text: "Chart area"
                            color: root.textColor
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }

                        Canvas {
                            id: chart
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = "#7A4F39"
                                ctx.lineWidth = 3
                                ctx.beginPath()
                                for (let x = 0; x < width; x += 12) {
                                    const y = height / 2 + Math.sin(x / 32) * 44 + Math.cos(x / 18) * 12
                                    if (x === 0)
                                        ctx.moveTo(x, y)
                                    else
                                        ctx.lineTo(x, y)
                                }
                                ctx.stroke()
                            }
                        }
                    }
                }
            }
        }
    }
}
