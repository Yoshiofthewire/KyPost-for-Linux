import QtQuick 2.15
import com.urlxl.LlamaMail 1.0

// STYLE_GUIDE.md §4 "Circular gradient avatar with initials" (mirrors
// web's `.users-avatar`, `.contacts-avatar`): circle, two-stop gradient
// fill from Theme.accent to Theme.accentSoft, 1px border, initials text in
// Theme.readableOnAccent. STYLE_GUIDE gives two concrete reference sizes --
// 34 for list rows, 52 for detail headers -- so `size` is a plain property
// rather than a hardcoded dimension; callers pass either.
//
// Judgment call: STYLE_GUIDE doesn't specify the border color explicitly.
// Theme.line is used here -- the same "structural outline" role it plays
// on GhostButton's stroke and PillTab's inactive border -- rather than
// inventing a new semantic color for a single use site.
//
// Judgment call: a plain QtQuick `Gradient`/`GradientStop` pair on a
// circular Rectangle (radius: width / 2) is used instead of
// Qt5Compat.GraphicalEffects / QtQuick.Effects RadialGradient or
// LinearGradient -- neither module is a project dependency (see
// app/CMakeLists.txt), and a top-to-bottom two-stop Gradient reads as a
// reasonable "gradient avatar" without adding one.
Rectangle {
    id: root

    // Public API.
    property string initials: ""
    property int size: 34
    // extended-contact-fields Task 3: a file:// URL (or "") to a cached
    // contact photo -- see ContactsController::photoPathFor(). Empty string
    // (the default) means "no photo available", which keeps the gradient +
    // initials rendering below exactly as it was before this property
    // existed; every existing caller that never sets this is unaffected.
    property string photoSource: ""

    width: root.size
    height: root.size
    radius: width / 2
    border.width: 1
    border.color: Theme.line
    // Needed so the Image below (a plain rectangular Image) is visually
    // clipped down to this Rectangle's circular radius instead of showing
    // square corners poking out past the circle.
    clip: true

    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.accent }
        GradientStop { position: 1.0; color: Theme.accentSoft }
    }

    Image {
        anchors.fill: parent
        source: root.photoSource
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        smooth: true
        visible: root.photoSource !== ""
    }

    Text {
        anchors.centerIn: parent
        text: root.initials
        color: Theme.readableOnAccent
        font.family: Theme.fontUi
        font.pixelSize: Math.round(root.size * 0.4)
        font.weight: Font.Medium
        // Falls back to initials whenever there's no photo -- unchanged
        // behavior from before this property existed, just now made
        // explicit via a `visible` binding instead of being the only
        // possible rendering.
        visible: root.photoSource === ""
    }
}
