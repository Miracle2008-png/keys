import QtQuick
import QtQuick.Controls

// Real QML: properties, bindings, signals, states.
Rectangle {
    id: root

    property int count: 0
    property alias label: text.text
    readonly property bool active: count > 0

    signal activated(int value)

    width: 200; height: 100
    color: active ? "#3d7eff" : "#1a1d21"

    Behavior on color {
        ColorAnimation { duration: 150 }
    }

    Text {
        id: text
        anchors.centerIn: parent
        text: qsTr("Count: %1").arg(root.count)
    }

    MouseArea {
        anchors.fill: parent
        onClicked: { root.count++; root.activated(root.count); }
    }
}
