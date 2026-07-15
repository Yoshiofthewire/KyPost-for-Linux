import QtQuick 2.15
import QtQuick.Layouts 1.15
import com.urlxl.LlamaMail 1.0
import "../components"

// Task 36 -- plain reusable Item, deliberately NOT a Kirigami.Page (see
// Phase 6 global constraint 4): MobileRoot wraps this in a thin
// Kirigami.Page shell when it pushes it (Task 38); DesktopRoot embeds it
// directly inside its list-pane Item (Task 39). Navigation is signal-based,
// same "don't assume how the host navigates" shape as EmailDetail/Compose
// (Task 35) -- tapping a row or "Add" both just report a uid, they never
// push/pop anything themselves.
Item {
    id: root

    // uid === "" means "Add" (create mode) -- ContactDetail.qml's own `uid`
    // property already treats an empty string this way (see its doc
    // comment), so "Add" reuses this single signal with the "" sentinel
    // rather than adding a second addRequested() signal that would carry no
    // real behavioral difference for whichever root ends up handling it.
    signal contactSelected(string uid)

    implicitWidth: 360
    implicitHeight: 640

    Component.onCompleted: ContactsApp.load()

    // Shown for 3s after a Sync tap, then hidden again -- ContactsApp.
    // statusMessage/lastError are ContactsApp's own persistent last-outcome
    // state (they don't clear themselves), so "briefly after" is
    // implemented as a local visibility flag here rather than this file
    // reaching in and blanking properties it doesn't own.
    property bool showSyncStatus: false

    // Same "up to 2 characters from whitespace-split name parts" shape as
    // EmailDetail.qml's initialsFor() (Task 35) -- kept as a local helper
    // rather than promoted anywhere shared: ContactDetail.qml needs the
    // exact same logic but the two files have no other coupling, and this
    // is a handful of lines, not something worth a shared JS module for.
    function initialsFor(fn) {
        const s = (fn || "").trim()
        if (s.length === 0)
            return "?"
        const parts = s.split(/\s+/).filter(function (p) { return p.length > 0 })
        let initials = ""
        for (let i = 0; i < parts.length && initials.length < 2; i++)
            initials += parts[i].charAt(0).toUpperCase()
        return initials
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: i18n("Contacts")
                color: Theme.inkStrong
                font.family: Theme.fontUi
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            GhostButton {
                text: i18n("Add")
                onClicked: root.contactSelected("")
            }
            PrimaryButton {
                text: i18n("Sync")
                enabled: !ContactsApp.isBusy
                onClicked: {
                    ContactsApp.sync()
                    root.showSyncStatus = true
                    syncStatusTimer.restart()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.showSyncStatus && (ContactsApp.lastError !== "" || ContactsApp.statusMessage !== "")
            text: ContactsApp.lastError !== "" ? ContactsApp.lastError : ContactsApp.statusMessage
            color: ContactsApp.lastError !== "" ? Theme.dangerColor : Theme.ink
            font.family: Theme.fontUi
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: listView.count === 0
            text: i18n("No contacts yet — sync or add one.")
        }

        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: count > 0
            clip: true
            spacing: 4
            model: ContactsApp.contactModel

            delegate: Rectangle {
                width: listView.width
                height: rowContent.implicitHeight + 20
                radius: Theme.shapeButton
                color: tapHandler.pressed ? Theme.panel : "transparent"

                Behavior on color {
                    ColorAnimation { duration: 120 }
                }

                RowLayout {
                    id: rowContent
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 12

                    Avatar {
                        initials: root.initialsFor(model.fn)
                        size: 34
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: model.fn && model.fn.length > 0 ? model.fn : i18n("Unnamed")
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: model.primaryEmail !== ""
                            text: model.primaryEmail
                            color: Theme.ink
                            font.family: Theme.fontUi
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }
                }

                TapHandler {
                    id: tapHandler
                    onTapped: root.contactSelected(model.uid)
                }
            }
        }
    }

    Timer {
        id: syncStatusTimer
        interval: 3000
        onTriggered: root.showSyncStatus = false
    }
}
