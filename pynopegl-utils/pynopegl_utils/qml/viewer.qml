/*
 * Copyright 2023 Clément Bœsch <u pkh.me>
 * Copyright 2023 Nope Forge
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

ApplicationWindow {
    id: root
    visible: true
    minimumWidth: 1280
    minimumHeight: 800

    signal selectScript(string script_name)
    signal selectScene(int index)
    signal selectFramerate(int index)
    signal selectExportFile(string export_file)
    signal selectExportResolution(int index)
    signal selectExportProfile(string profile_id)
    signal selectExportSamples(int index)
    signal exportVideo(string filename, int res, string profile_id, int samples)
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
        paramList.widget_model = params;
    }

    function set_controls_model(controls) {
        controlList.widget_model = controls;
    }

    function set_export_file(export_file) {
        exportFile.text = export_file;
        selectExportFile(export_file);
    }

    function get_export_file() {
        return exportFile.text;
    }

    function set_export_resolutions(resolutions, current_index) {
        resList.model = resolutions;
        resList.currentIndex = current_index;
    }

    function set_export_profiles(profiles, current_index) {
        profileList.model = profiles;
        profileList.currentIndex = current_index;
    }

    function set_export_samples(samples, current_index) {
        samplesList.model = samples;
        samplesList.currentIndex = current_index;
    }

    function show_export_warning(warning) {
        exportWarning.visible = true;
        exportWarning.text = warning;
    }

    function hide_export_warning() {
        exportWarning.visible = false;
    }

    function set_error(error) {
        errorText.text = error;
    }

    AboutPopup { id: aboutPopup }

    function set_about_content(text) { aboutPopup.set_about_content(text); }

    FileDialog {
        id: scriptDialog
        nameFilters: ["Python scripts (*.py)"]
        onAccepted: set_script(selectedFile)
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
        onAccepted: set_export_file(selectedFile)
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
        infoPopup.visible = true;
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

    function start_export(filename, res, profile_id, samples) {
        exportBar.value = 0;
        exportStack.currentIndex = 0;
        exportPopup.visible = true;
        exportVideo(filename, res, profile_id, samples);
    }

    function finish_export() {
        exportStack.currentIndex = 1;
    }

    function abort_export(msg) {
        exportErrorText.text = msg
        exportStack.currentIndex = 2;
    }

    function disable_export(msg) {
        exportQueryError.text = msg;
        exportQueryStack.currentIndex = 1;
    }

    function set_export_progress(progress) {
        exportBar.value = progress;
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => set_script(drop.urls[0])
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

            SceneWidgets {
                id: paramList
                box_title: "Build scene options"
                window: root
            }

            SceneWidgets {
                id: controlList
                box_title: "Live scene controls"
                window: root
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
                                TextField {
                                    id: exportFile
                                    Layout.fillWidth: true
                                    onEditingFinished: selectExportFile(text)
                                }
                                Button {
                                    text: "Open"
                                    background.implicitWidth: 0
                                    onClicked: exportDialog.open()
                                }
                            }

                            Label { text: "Res:" }
                            ComboBox {
                                id: resList
                                Layout.fillWidth: true
                                onActivated: selectExportResolution(currentIndex)
                            }

                            Label { text: "Profile:" }
                            ComboBox {
                                id: profileList
                                Layout.fillWidth: true
                                onActivated: selectExportProfile(currentValue)
                                valueRole: "profile_id"
                                textRole: "title"
                            }

                            Label { text: "MSAA:" }
                            ComboBox {
                                id: samplesList
                                Layout.fillWidth: true
                                onActivated: selectExportSamples(currentIndex)
                            }
                        }
                        Label { id: exportWarning; visible: false; color: "red" }
                        Button {
                            text: "Export"
                            Layout.alignment: Qt.AlignHCenter
                            onClicked: start_export(exportFile.text, resList.currentIndex, profileList.currentValue, samplesList.currentIndex)
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
                    readOnly: true
                    id: errorText
                    font.family: "monospace"
                    color: "red"
                }
            }
        }
    }
}
