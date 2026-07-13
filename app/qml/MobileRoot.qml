import QtQuick 2.15
import org.kde.kirigami 2.20 as Kirigami

Kirigami.ApplicationWindow {
    id: root
    width: 360
    height: 640
    visible: true

    pageStack.initialPage: Kirigami.Page {
        Kirigami.Heading {
            anchors.centerIn: parent
            text: "Llama Mail — Mobile"
        }
    }
}
