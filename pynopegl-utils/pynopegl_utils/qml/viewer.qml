/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
 * Copyright 2023 Nope Foundry
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Templates as T
import Qt.labs.qmlmodels // DelegateChooser is experimental

ApplicationWindow {
    visible: true
    minimumWidth: 1280
    minimumHeight: 800

    signal selectScript(string script_name)
    signal selectScene(int index)
    signal selectFramerate(int index)
    signal exportVideo(string filename, int res, int profile, int samples)
    signal cancelExport()

    function set_script(script_name) {
        script.text = script_name;
        selectScript(script_name);
    }

    function set_scenes(scenes, current_index) {
        sceneList.model = scenes;
        sceneList.currentIndex = current_index;
        selectScene(current_index);
    }

    function set_framerates(framerates, current_index) {
        framerateList.model = framerates;
        framerateList.currentIndex = current_index;
        selectFramerate(current_index);
    }

    function set_params_model(params) {
        paramList.model = params;
    }

    Popup {
        id: aboutPopup
        modal: true
        padding: 20
        anchors.centerIn: parent
        implicitWidth: 640
        implicitHeight: 480

        contentItem: ColumnLayout {
            spacing: 20
            TextArea {
                id: aboutContent
                Layout.fillWidth: true
                Layout.fillHeight: true
                readOnly: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.MarkdownText
                onLinkActivated: (link) => Qt.openUrlExternally(link)

                // Background is specified because of https://bugreports.qt.io/browse/QTBUG-105858
                background: Rectangle { radius: 2; color: "white"; border.color: "#aaa" }

                HoverHandler {
                    enabled: parent.hoveredLink
                    cursorShape: Qt.PointingHandCursor
                }
            }
            Button {
                text: "OK"
                onClicked: aboutPopup.visible = false
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    function set_about_content(text) { aboutContent.text = text; }

    FileDialog {
        id: scriptDialog
        nameFilters: ["Python scripts (*.py)"]
        onAccepted: {
            script.text = selectedFile
            script.editingFinished()
        }
    }

    function set_script_directory(dir) {
        scriptDialog.currentFolder = dir;
    }

    /*
     * Export
     */

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        onAccepted: {
            exportFile.text = selectedFile
            exportFile.editingFinished()
        }
    }

    function set_export_name_filters(name_filters) {
        exportDialog.nameFilters = name_filters;
    }

    function set_export_directory(dir) {
        exportDialog.currentFolder = dir;
    }

    MessageDialog {
        id: infoPopup
        title: "Export information"
        onAccepted: visible = false
    }

    function show_popup(msg) {
        infoPopup.text = msg;
        infoPopup.visible = true
    }

    Popup {
        id: exportPopup

        modal: true
        closePolicy: Popup.NoAutoClose

        anchors.centerIn: parent
        StackLayout {
            id: exportStack

            // 0: Exporting
            ColumnLayout {
                Label { text: "Exporting " + exportFile.text }
                ProgressBar {
                    id: exportBar
                    Layout.fillWidth: true
                }
                Button {
                    text: "Cancel"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        cancelExport()
                        exportPopup.visible = false
                    }
                }
            }

            // 1: Export succeed
            ColumnLayout {
                Item { Layout.fillWidth: true; Layout.fillHeight: true } // spacer
                Label { text: exportFile.text + " export done." }
                Item { Layout.fillWidth: true; Layout.fillHeight: true } // spacer
                Button {
                    text: "OK"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: exportPopup.visible = false
                }
            }

            // 2: Export failed
            ColumnLayout {
                Label { text: exportFile.text + " export failed."; color: "red" }
                Label { id: exportErrorText }
                Item { Layout.fillWidth: true; Layout.fillHeight: true } // spacer
                Button {
                    text: "OK"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: exportPopup.visible = false
                }
            }
        }
    }

    function start_export(filename, res, profile, samples) {
        exportBar.value = 0
        exportStack.currentIndex = 0
        exportPopup.visible = true
        exportVideo(filename, res, profile, samples)
    }

    function finish_export() {
        exportStack.currentIndex = 1
    }

    function abort_export(msg) {
        exportErrorText.text = msg
        exportStack.currentIndex = 2
    }

    function disable_export(msg) {
        exportQueryError.text = msg
        exportQueryStack.currentIndex = 1
    }

    function set_export_progress(progress) {
        exportBar.value = progress;
    }

    /*
     * Main view
     */

    SplitView {
        anchors.fill: parent

        // TODO: implement a ScrollView that doesn't mess up everything
        ColumnLayout {
            SplitView.fillWidth: false

            Frame {
                Layout.fillWidth: true
                Layout.margins: 5

                contentItem: GridLayout {
                    columns: 2

                    Label { text: "Script:" }
                    RowLayout {
                        TextField {
                            id: script
                            Layout.fillWidth: true
                            onEditingFinished: selectScript(text)
                        }
                        Button {
                            text: "Open"
                            background.implicitWidth: 0
                            onClicked: scriptDialog.open()
                        }
                    }

                    Label { text: "Scene:" }
                    ComboBox {
                        id: sceneList
                        Layout.fillWidth: true
                        onActivated: selectScene(currentIndex)
                    }
                }
            }

            GroupBox {
                title: "Rendering settings"
                Layout.fillWidth: true
                Layout.margins: 5

                contentItem: GridLayout {
                    columns: 2

                    Label { text: "Framerate:" }
                    ComboBox {
                        id: framerateList
                        Layout.fillWidth: true
                        onActivated: selectFramerate(currentIndex)
                    }
                }
            }

            GroupBox {
                title: "Build scene options"
                Layout.fillWidth: true
                Layout.margins: 5
                visible: paramList.count > 0

                contentItem: ListView {
                    id: paramList
                    implicitWidth: contentItem.childrenRect.width
                    implicitHeight: contentHeight
                    clip: true
                    spacing: 5

                    delegate: DelegateChooser {
                        role: "type"

                        DelegateChoice {
                            roleValue: "range"
                            RowLayout {
                                property bool fixed: model.step == 1.0
                                Label { text: model.label }
                                Slider {
                                    snapMode: Slider.SnapAlways
                                    stepSize: model.step
                                    from: model.min
                                    to: model.max
                                    value: model.val
                                    onMoved: model.val = fixed ? (value | 0) : value
                                }
                                Label { text: model.val.toFixed(fixed ? 0 : 3) }
                            }
                        }

                        DelegateChoice {
                            roleValue: "vector"

                            RowLayout {
                                property var mdl: model

                                Label { text: model.label }
                                Repeater {
                                    model: mdl.n

                                    // This code is directly derived form the documentation snippet...
                                    // https://doc.qt.io/Qt-6/qml-qtquick-controls-spinbox.html
                                    SpinBox {
                                        id: spinBox
                                        from: decimalToInt(mdl.min[index])
                                        value: decimalToInt(mdl.val[index])
                                        to: decimalToInt(mdl.max[index])
                                        stepSize: decimalToInt(0.01)
                                        editable: true
                                        background.implicitWidth: 0

                                        property int decimals: 2
                                        property real realValue: value / decimalFactor
                                        readonly property int decimalFactor: Math.pow(10, decimals)

                                        function decimalToInt(decimal) {
                                            return decimal * decimalFactor
                                        }

                                        validator: DoubleValidator {
                                            bottom: Math.min(spinBox.from, spinBox.to)
                                            top:  Math.max(spinBox.from, spinBox.to)
                                            decimals: spinBox.decimals
                                            notation: DoubleValidator.StandardNotation
                                        }

                                        textFromValue: function(value, locale) {
                                            return Number(value / decimalFactor).toLocaleString(locale, 'f', spinBox.decimals)
                                        }

                                        valueFromText: function(text, locale) {
                                            return Math.round(Number.fromLocaleString(locale, text) * decimalFactor)
                                        }

                                        onValueModified: {
                                            // mdl.val[index] = realValue has no effect so we do this dance
                                            let val = mdl.val
                                            val[index] = realValue
                                            mdl.val = val
                                        }
                                    }
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: "color"
                            Button {
                                ColorDialog {
                                    id: color_dialog
                                    onSelectedColorChanged: model.val = selectedColor
                                    Component.onCompleted: selectedColor = model.val
                                    options: ColorDialog.NoButtons
                                }
                                text: model.label
                                palette.button: color_dialog.selectedColor
                                onClicked: color_dialog.open()
                            }
                        }

                        DelegateChoice {
                            roleValue: "bool"
                            Switch {
                                checked: model.val
                                onToggled: model.val = checked
                                text: model.label
                            }
                        }

                        DelegateChoice {
                            roleValue: "file"
                            RowLayout {
                                FileDialog {
                                    id: file_dialog
                                    nameFilters: [model.filter]
                                    onAccepted: model.val = selectedFile.toString()
                                }
                                Label { text: model.label }
                                TextField {
                                    id: file_text
                                    text: model.val ? model.val : ""
                                    Layout.fillWidth: true
                                    onEditingFinished: model.val = text
                                }
                                Button {
                                    text: "Open"
                                    background.implicitWidth: 0
                                    onClicked: file_dialog.open()
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: "list"
                            RowLayout {
                                // this is needed to avoid confusion between
                                // the element "model" entry and the ComboBox.model.
                                // That's also why we don't use model.val here.
                                property var _choices: model.choices
                                Label { text: label + ":" }
                                ComboBox {
                                    model: _choices
                                    currentIndex: _choices.indexOf(val)
                                    onActivated: val = _choices[currentIndex]
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: "text"
                            RowLayout {
                                Label { text: model.label + ":" }
                                TextField {
                                    text: model.val
                                    onEditingFinished: model.val = text
                                }
                            }
                        }
                    }
                }
            }

            GroupBox {
                title: "Live scene controls"
                Layout.fillWidth: true
                Layout.margins: 5
                visible: controlList.count > 0

                contentItem: ListView {
                    id: controlList
                    objectName: "controlList"

                    implicitWidth: contentItem.childrenRect.width
                    implicitHeight: contentHeight
                    clip: true
                    spacing: 5

                    delegate: DelegateChooser {
                        role: "type"

                        DelegateChoice {
                            roleValue: "range"
                            RowLayout {
                                property bool fixed: model.step == 1.0
                                Label { text: model.label }
                                Slider {
                                    snapMode: Slider.SnapAlways
                                    stepSize: model.step
                                    from: model.min
                                    to: model.max
                                    value: model.val
                                    onMoved: model.val = fixed ? (value | 0) : value
                                }
                                Label { text: model.val.toFixed(fixed ? 0 : 3) }
                            }
                        }

                        DelegateChoice {
                            roleValue: "vector"

                            RowLayout {
                                property var mdl: model

                                Label { text: model.label }
                                Repeater {
                                    model: mdl.n

                                    // This code is directly derived form the documentation snippet...
                                    // https://doc.qt.io/Qt-6/qml-qtquick-controls-spinbox.html
                                    SpinBox {
                                        id: spinBox
                                        from: decimalToInt(mdl.min[index])
                                        value: decimalToInt(mdl.val[index])
                                        to: decimalToInt(mdl.max[index])
                                        stepSize: decimalToInt(0.01)
                                        editable: true
                                        background.implicitWidth: 0

                                        property int decimals: 2
                                        property real realValue: value / decimalFactor
                                        readonly property int decimalFactor: Math.pow(10, decimals)

                                        function decimalToInt(decimal) {
                                            return decimal * decimalFactor
                                        }

                                        validator: DoubleValidator {
                                            bottom: Math.min(spinBox.from, spinBox.to)
                                            top:  Math.max(spinBox.from, spinBox.to)
                                            decimals: spinBox.decimals
                                            notation: DoubleValidator.StandardNotation
                                        }

                                        textFromValue: function(value, locale) {
                                            return Number(value / decimalFactor).toLocaleString(locale, 'f', spinBox.decimals)
                                        }

                                        valueFromText: function(text, locale) {
                                            return Math.round(Number.fromLocaleString(locale, text) * decimalFactor)
                                        }

                                        onValueModified: {
                                            // mdl.val[index] = realValue has no effect so we do this dance
                                            let val = mdl.val
                                            val[index] = realValue
                                            mdl.val = val
                                        }
                                    }
                                }
                            }
                        }

                        DelegateChoice {
                            roleValue: "color"
                            Button {
                                ColorDialog {
                                    id: color_dialog
                                    onSelectedColorChanged: model.val = selectedColor
                                    Component.onCompleted: selectedColor = model.val
                                    options: ColorDialog.NoButtons
                                }
                                text: model.label
                                palette.button: color_dialog.selectedColor
                                onClicked: color_dialog.open()
                            }
                        }

                        DelegateChoice {
                            roleValue: "bool"
                            Switch {
                                checked: model.val
                                onToggled: model.val = checked
                                text: model.label
                            }
                        }

                        DelegateChoice {
                            roleValue: "text"
                            RowLayout {
                                Label { text: model.label + ":" }
                                TextField {
                                    text: model.val
                                    onEditingFinished: model.val = text
                                }
                            }
                        }
                    }
                }
            }

            GroupBox {
                title: "Export as video"
                Layout.fillWidth: true
                Layout.margins: 5

                contentItem: StackLayout {
                    id: exportQueryStack
                    clip: true

                    // 0: Export form
                    ColumnLayout {
                        GridLayout {
                            columns: 2

                            Label { text: "File:" }
                            RowLayout {
                                TextField { id: exportFile; objectName: "exportFile"; Layout.fillWidth: true }
                                Button {
                                    text: "Open"
                                    background.implicitWidth: 0
                                    onClicked: exportDialog.open()
                                }
                            }

                            Label { text: "Res:" }
                            ComboBox { id: resList; objectName: "resList"; Layout.fillWidth: true }

                            Label { text: "Profile:" }
                            ComboBox { id: profileList; objectName: "profileList"; Layout.fillWidth: true }

                            Label { text: "MSAA:" }
                            ComboBox { id: samplesList; objectName: "samplesList"; Layout.fillWidth: true }

                        }
                        Label {
                            objectName: "exportWarning"
                            visible: false
                            color: "red"
                        }
                        Button {
                            text: "Export"
                            Layout.alignment: Qt.AlignHCenter
                            onClicked: start_export(exportFile.text, resList.currentIndex, profileList.currentIndex, samplesList.currentIndex)
                        }
                    }

                    // 1: Export not available
                    Label { id: exportQueryError }
                }
            }

            /* Spacer */
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Button {
                text: "About"
                onClicked: aboutPopup.open()
                Layout.alignment: Qt.AlignHCenter
                Layout.margins: 5
            }
        }

        ColumnLayout {
            Player {
                Layout.margins: 5
                SplitView.fillWidth: true
                objectName: "player"
            }

            GroupBox {
                Layout.margins: 5
                Layout.fillWidth: true
                visible: errorText.text != ""
                TextEdit {
                    objectName: "errorText"
                    readOnly: true
                    id: errorText
                    font.family: "monospace"
                    color: "red"
                }
            }
        }
    }
}
