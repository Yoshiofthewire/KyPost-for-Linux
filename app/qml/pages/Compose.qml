import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import com.urlxl.LlamaMail 1.0
import "../components"

// Task 35 -- plain reusable Item, deliberately NOT a Kirigami.Page (see
// Phase 6 global constraint 4); MobileRoot/DesktopRoot each host this
// directly (Tasks 38/39). Plain text only, no rich-text toolbar anywhere in
// this file (global constraint 5) -- Body is a bare TextArea.
Item {
    id: root

    // Public API -- pre-fill values, set by EmailDetail.qml for reply/
    // reply-all/forward, left empty for a fresh compose.
    property string initialTo: ""
    property string initialSubject: ""
    property string initialBody: ""

    // Emitted once MailApp.sendMail() reports success -- MobileRoot/
    // DesktopRoot are expected to pop/close this screen in response; this
    // component doesn't assume push-navigation vs a pane, per constraint 4.
    signal sendSucceeded()

    implicitWidth: 360
    implicitHeight: 640

    property var attachmentPaths: []
    property string validationError: ""

    Component.onCompleted: {
        toField.text = root.initialTo
        subjectField.text = root.initialSubject
        bodyArea.text = root.initialBody
    }

    function trySend() {
        // Mirrors Android's "Please fill in all fields" check -- Cc/Bcc
        // stay optional, only To/Subject/Body are required.
        if (toField.text.trim() === "" || subjectField.text.trim() === "" || bodyArea.text.trim() === "") {
            root.validationError = i18n("Please fill in all fields")
            return
        }
        root.validationError = ""
        const ok = MailApp.sendMail(toField.text, ccField.text, bccField.text, subjectField.text,
                                     bodyArea.text, root.attachmentPaths)
        if (ok)
            root.sendSucceeded()
    }

    function fileNameOf(path) {
        const parts = path.split("/")
        return parts[parts.length - 1]
    }

    // FileDialog's selectedFiles are QML `url` values ("file:///home/…",
    // percent-encoded) -- MailApp.sendMail()'s attachmentFilePaths expects
    // plain local filesystem paths (it hands each one straight to QFile),
    // so strip the scheme and percent-decode here rather than passing the
    // url string through as-is.
    function urlToLocalPath(fileUrl) {
        let s = fileUrl.toString()
        if (s.indexOf("file://") === 0)
            s = s.substring(7)
        return decodeURIComponent(s)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        ThemedTextField {
            id: toField
            Layout.fillWidth: true
            placeholderText: i18n("To")
        }
        ThemedTextField {
            id: ccField
            Layout.fillWidth: true
            placeholderText: i18n("Cc")
        }
        ThemedTextField {
            id: bccField
            Layout.fillWidth: true
            placeholderText: i18n("Bcc")
        }
        ThemedTextField {
            id: subjectField
            Layout.fillWidth: true
            placeholderText: i18n("Subject")
        }

        // Body -- plain multi-line TextArea (global constraint 5: no
        // rich-text toolbar/component anywhere in this screen). Wrapped in
        // a themed Rectangle + Flickable rather than promoted to a shared
        // component: this is the only multi-line field on this screen, so
        // there's nothing to de-duplicate the way ThemedTextField
        // de-duplicates the four single-line fields above.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.shapeField
            color: Theme.panel
            border.width: 1
            border.color: bodyArea.activeFocus ? Theme.accent : Theme.line

            Behavior on border.color {
                ColorAnimation { duration: 120 }
            }

            Flickable {
                anchors.fill: parent
                anchors.margins: 10
                contentWidth: width
                contentHeight: bodyArea.implicitHeight
                clip: true

                TextArea {
                    id: bodyArea
                    width: parent.width
                    wrapMode: TextArea.Wrap
                    color: Theme.inkStrong
                    placeholderTextColor: Theme.ink
                    font.family: Theme.fontUi
                    font.pixelSize: 14
                    background: null
                    selectByMouse: true
                    placeholderText: i18n("Write your message…")
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            GhostButton {
                text: i18n("Attach files")
                onClicked: fileDialog.open()
            }
            Item { Layout.fillWidth: true }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            visible: root.attachmentPaths.length > 0

            Repeater {
                model: root.attachmentPaths
                delegate: Rectangle {
                    radius: Theme.shapeButton
                    color: Theme.panel
                    border.width: 1
                    border.color: Theme.line
                    implicitWidth: chipRow.implicitWidth + 20
                    implicitHeight: chipRow.implicitHeight + 12

                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: root.fileNameOf(modelData)
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 12
                        }
                        Text {
                            text: "✕"
                            color: Theme.ink
                            font.family: Theme.fontUi
                            font.pixelSize: 12

                            TapHandler {
                                onTapped: {
                                    const updated = root.attachmentPaths.slice()
                                    updated.splice(index, 1)
                                    root.attachmentPaths = updated
                                }
                            }
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.validationError !== "" || MailApp.lastError !== ""
            text: root.validationError !== "" ? root.validationError : MailApp.lastError
            color: Theme.dangerColor
            font.family: Theme.fontUi
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                visible: MailApp.isBusy
                text: i18n("Sending…")
                color: Theme.ink
                font.family: Theme.fontUi
                font.pixelSize: 13
            }
            Item { Layout.fillWidth: true }
            PrimaryButton {
                text: i18n("Send")
                enabled: !MailApp.isBusy
                onClicked: root.trySend()
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: i18n("Attach files")
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            const paths = []
            for (let i = 0; i < selectedFiles.length; i++)
                paths.push(root.urlToLocalPath(selectedFiles[i]))
            root.attachmentPaths = root.attachmentPaths.concat(paths)
        }
    }
}
