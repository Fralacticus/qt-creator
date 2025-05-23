// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

import QtQuick
import QtQuick.Templates as T

import StudioTheme as StudioTheme

T.TextField {
    id: control

    property StudioTheme.ControlStyle style: StudioTheme.Values.controlStyle

    // This property is used to indicate the global hover state
    property bool hover: mouseArea.containsMouse && control.enabled
    property bool edit: control.activeFocus

    property string preFocusText: ""

    signal rejected

    horizontalAlignment: Qt.AlignLeft
    verticalAlignment: Qt.AlignVCenter

    font.pixelSize: control.style.baseFontSize

    color: control.style.text.idle
    selectionColor: control.style.text.selection
    selectedTextColor: control.style.text.selectedText
    placeholderTextColor: control.style.text.placeholder

    readOnly: false
    selectByMouse: true
    persistentSelection: control.focus //|| contextMenu.visible

    width: control.style.controlSize.width
    height: control.style.controlSize.height
    implicitHeight: control.style.controlSize.height

    leftPadding: control.style.inputHorizontalPadding
    rightPadding: control.style.inputHorizontalPadding

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: true
        hoverEnabled: true
        propagateComposedEvents: true
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.PointingHandCursor
    }

    onPressed: function(event) {
        //if (event.button === Qt.RightButton)
        //    contextMenu.popup(control)
    }

    onActiveFocusChanged: {
        // OtherFocusReason in this case means, if the TextField gets focus after the context menu
        // was closed due to an menu item click.
        // TODO focusReason needs to be ignored, otherwise will end up with empty TextField
        if (control.activeFocus)// && control.focusReason !== Qt.OtherFocusReason)
            control.preFocusText = control.text

        if (!control.activeFocus)
            control.deselect()
    }

    onEditChanged: {
        //if (control.edit)
        //    contextMenu.close()
    }

    //onEditingFinished: control.focus = false

    Text {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)

        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText
                 && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
        elide: Text.ElideRight
        renderType: control.renderType
    }

    background: Rectangle {
        id: textFieldBackground
        color: control.style.background.idle
        border.color: control.style.border.idle
        border.width: control.style.borderWidth
        x: 0
        width: control.width
        height: control.height
    }

    states: [
        State {
            name: "default"
            when: control.enabled && !control.hover && !control.edit// && !contextMenu.visible
            PropertyChanges {
                target: textFieldBackground
                color: control.style.background.idle
                border.color: control.style.border.idle
            }
            PropertyChanges {
                target: control
                color: control.style.text.idle
                placeholderTextColor: control.style.text.placeholder
            }
            PropertyChanges {
                target: mouseArea
                cursorShape: Qt.PointingHandCursor
            }
        },
        State {
            name: "globalHover"
            when: !control.edit && control.enabled// && !contextMenu.visible
            PropertyChanges {
                target: textFieldBackground
                color: control.style.background.globalHover
                border.color: control.style.border.idle
            }
            PropertyChanges {
                target: control
                color: control.style.text.idle
                placeholderTextColor: control.style.text.placeholder
            }
        },
        State {
            name: "hover"
            when: mouseArea.containsMouse && !control.edit && control.enabled// && !contextMenu.visible
            PropertyChanges {
                target: textFieldBackground
                color: control.style.background.hover
                border.color: control.style.border.hover
            }
            PropertyChanges {
                target: control
                color: control.style.text.hover
                placeholderTextColor: control.style.text.placeholder
            }
        },
        State {
            name: "edit"
            when: control.edit// || contextMenu.visible
            PropertyChanges {
                target: textFieldBackground
                color: control.style.background.interaction
                border.color: control.style.border.interaction
            }
            PropertyChanges {
                target: control
                color: control.style.text.idle
                placeholderTextColor: control.style.text.placeholder
            }
            PropertyChanges {
                target: mouseArea
                cursorShape: Qt.IBeamCursor
            }
        },
        State {
            name: "disable"
            when: !control.enabled
            PropertyChanges {
                target: textFieldBackground
                color: control.style.background.disabled
                border.color: control.style.border.disabled
            }
            PropertyChanges {
                target: control
                color: control.style.text.disabled
                placeholderTextColor: control.style.text.disabled
            }
        }
    ]

    Keys.onEscapePressed: function(event) {
        event.accepted = true
        control.text = control.preFocusText
        control.rejected()
        control.focus = false
    }
}
