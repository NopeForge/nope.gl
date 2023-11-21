/*
 * Copyright 2022 GoPro Inc.
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
    visible: true
    minimumWidth: 1280
    minimumHeight: 800

    property string filename0
    property string filename1
    property int width0
    property int width1
    property int height0
    property int height1
    property string pix_fmt0
    property string pix_fmt1
    property real duration0
    property real duration1
    property real avg_frame_rate0
    property real avg_frame_rate1

    signal diffModeToggled(bool checked)
    signal verticalSplitChanged(bool enabled)
    signal thresholdMoved(real value)
    signal showCompChanged(int comp, bool checked)
    signal premultipliedChanged(bool enabled)
    signal setFile(int file_id, string filename)

    function get_diff_mode() { return diff_mode.checked; }
    function get_vertical_split() { return vertical_split.checked; }
    function get_threshold() { return threshold.value; }
    function get_show_r() { return show_r.checked; }
    function get_show_g() { return show_g.checked; }
    function get_show_b() { return show_b.checked; }
    function get_show_a() { return show_a.checked; }
    function get_premultiplied() { return premultiplied.checked; }

    function set_media_directory(index, directory) {
        if (index == 0)
            mediaDialog0.currentFolder = directory;
        else if (index == 1)
            mediaDialog1.currentFolder = directory;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5

        RowLayout {
            Switch {
                id: diff_mode
                text: "Diff mode"
                checked: false
                onToggled: diffModeToggled(checked)
            }

            Button {
                id: vertical_split
                text: checked ? "|" : "―"
                checkable: true
                checked: true
                visible: !diff_mode.checked
                onClicked: verticalSplitChanged(checked)
                background.implicitWidth: 0
                background.implicitHeight: 0
            }

            Slider {
                id: threshold
                visible: diff_mode.checked
                from: 0
                to: 0.1
                value: 0.001
                onMoved: thresholdMoved(value)
            }

            Item { Layout.fillWidth: true } /* spacer */

            component ColorCompButton: Button {
                required property int index

                checked: true
                checkable: true
                onClicked: showCompChanged(index, checked)

                /* remove padding */
                background.implicitWidth: 0
                background.implicitHeight: 0
            }

            ColorCompButton { id: show_r; text: "R"; index: 0 }
            ColorCompButton { id: show_g; text: "G"; index: 1 }
            ColorCompButton { id: show_b; text: "B"; index: 2 }
            ColorCompButton { id: show_a; text: "A"; index: 3 }

            CheckBox {
                id: premultiplied
                text: "Premultiplied"
                checked: false
                onClicked: premultipliedChanged(checked)
            }
        }

        RowLayout {
            Component {
                id: mediaInfo

                RowLayout {
                    readonly property string diffColor: "red"
                    readonly property bool bothSet: filename0 && filename1

                    Button {
                        background.implicitWidth: 0
                        background.implicitHeight: 0
                        text: "❌"
                        onClicked: setFile(media_id, "")
                    }
                    Label { text: filename }
                    Label {
                        text: width_ + "×" + height_
                        Binding on color { value: diffColor; when: bothSet && (width0 != width1 || height0 != height1) }
                    }
                    Label {
                        text: pix_fmt
                        Binding on color { value: diffColor; when: bothSet && pix_fmt0 != pix_fmt1 }
                    }
                    Label {
                        text: "D:" + duration.toFixed(2)
                        Binding on color { value: diffColor; when: bothSet && duration0 != duration1 }
                    }
                    Label {
                        text: avg_frame_rate.toFixed(2) + "FPS"
                        Binding on color { value: diffColor; when: bothSet && avg_frame_rate0 != avg_frame_rate1 }
                    }
                }
            }

            Component {
                id: mediaSelector
                RowLayout {
                    Button {
                        text: "Select media #" + media_id
                        onClicked: [mediaDialog0, mediaDialog1][media_id].open()
                    }
                }
            }

            component MediaDialog: FileDialog {
                required property int file_id
                onAccepted: setFile(file_id, selectedFile)
            }

            MediaDialog { id: mediaDialog0; file_id: 0 }
            MediaDialog { id: mediaDialog1; file_id: 1 }

            Loader {
                readonly property int media_id: 0
                sourceComponent: filename0 ? mediaInfo : mediaSelector
                property string filename: filename0
                property int width_: width0
                property int height_: height0
                property string pix_fmt: pix_fmt0
                property real duration: duration0
                property real avg_frame_rate: avg_frame_rate0
            }

            Item { Layout.fillWidth: true } /* spacer */

            Loader {
                readonly property int media_id: 1
                sourceComponent: filename1 ? mediaInfo : mediaSelector
                property string filename: filename1
                property int width_: width1
                property int height_: height1
                property string pix_fmt: pix_fmt1
                property real duration: duration1
                property real avg_frame_rate: avg_frame_rate1
            }
        }

        Player {
            objectName: "player"
        }
    }
}
