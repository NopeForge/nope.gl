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
import QtQuick.Layouts

ApplicationWindow {
    visible: true
    minimumWidth: 640
    minimumHeight: 480

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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5

        RowLayout {
            Switch {
                text: "Diff mode"
                id: "diff_mode"
                checked: false
                onToggled: diffModeToggled(checked)
            }

            Button {
                text: checked ? "|" : "―"
                checkable: true
                checked: true
                visible: !diff_mode.checked
                onClicked: verticalSplitChanged(checked)
                background.implicitWidth: 0
                background.implicitHeight: 0
            }

            Slider {
                visible: diff_mode.checked
                from: 0
                to: 0.1
                value: 0.001
                onMoved: thresholdMoved(value)
            }

            Item { Layout.fillWidth: true } /* spacer */

            Repeater {
                model: 4
                delegate: Button {
                    text: "RGBA"[index]
                    checked: true
                    checkable: true
                    onClicked: showCompChanged(index, checked)

                    /* remove padding */
                    background.implicitWidth: 0
                    background.implicitHeight: 0
                }
            }

            CheckBox {
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

                    Label { text: filename }
                    Label {
                        text: width_ + "×" + height_
                        Binding on color { value: diffColor; when: width0 != width1 || height0 != height1 }
                    }
                    Label {
                        text: pix_fmt
                        Binding on color { value: diffColor; when: pix_fmt0 != pix_fmt1 }
                    }
                    Label {
                        text: "D:" + duration.toFixed(2)
                        Binding on color { value: diffColor; when: duration0 != duration1 }
                    }
                    Label {
                        text: avg_frame_rate.toFixed(2) + "FPS"
                        Binding on color { value: diffColor; when: avg_frame_rate0 != avg_frame_rate1 }
                    }
                }
            }

            Loader {
                sourceComponent: mediaInfo
                property string filename: filename0
                property int width_: width0
                property int height_: height0
                property string pix_fmt: pix_fmt0
                property real duration: duration0
                property real avg_frame_rate: avg_frame_rate0
            }

            Item { Layout.fillWidth: true } /* spacer */

            Loader {
                sourceComponent: mediaInfo
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
