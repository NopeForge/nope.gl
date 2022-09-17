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
import QtQuick.Templates as T

ColumnLayout {

    /* Public fields */
    property var framerate: [60, 1]
    property real duration: 5
    property var aspect: [1, 1]

    signal timeChanged(real t)
    signal mouseDown(real x, real y)
    signal zoom(real angle_delta, real x, real y)
    signal pan(real vx, real vy)

    /* Private fields */
    property int timebase: 1000 // milliseconds
    property var clock_off: -1
    property real frame_ts: 0
    property int frame_index: 0
    readonly property int duration_i: parseInt(duration * framerate[0] / framerate[1])
    readonly property real frame_time: frame_index * framerate[1] / framerate[0]

    function clamp(v, min, max) {
        return Math.min(Math.max(v, min), max)
    }

    function sat(v) {
        return clamp(v, 0, 1)
    }

    function timed_text() {
        var cm = String(parseInt(frame_time / 60)).padStart(2, '0')
        var cs = String(parseInt(frame_time % 60)).padStart(2, '0')
        var dm = String(parseInt(duration / 60)).padStart(2, '0')
        var ds = String(parseInt(duration % 60)).padStart(2, '0')
        return cm + ":" + cs + " / " + dm + ":" + ds + " (" + frame_index + " @ " + framerate[0] + "/" + framerate[1] + ")"
    }

    function reset_running_time() {
        clock_off = Date.now() - frame_ts
    }

    function start_timer() {
        reset_running_time()
        timer.start()
    }

    function stop_timer() {
        timer.stop()
        reset_running_time()
    }

    function set_frame_ts(ts) {
        frame_ts = ts
        frame_index = parseInt(ts * framerate[0] / (framerate[1] * timebase))
        timeChanged(frame_time)
    }

    function set_frame_index(index) {
        frame_index = index
        frame_ts = parseInt(index * framerate[1] * (timebase / framerate[0]))
        timeChanged(frame_time)
    }

    function update_time(seek_at) {
        var now = Date.now()
        if (seek_at >= 0) {
            clock_off = now - seek_at
            set_frame_ts(seek_at)
            return
        }
        if (timer.running) {
            if (clock_off < 0 || now - clock_off > duration * timebase)
                clock_off = now
        }
        set_frame_ts(now - clock_off)
    }

    Rectangle {
        color: "#767676" /* Middle-gray for ideal color neutrality */
        Layout.fillWidth: true
        Layout.fillHeight: true

        Rectangle {
            objectName: "ngl_widget_placeholder"

            width: Math.min(parent.height * aspect[0] / aspect[1], parent.width)
            height: Math.min(parent.width * aspect[1] / aspect[0], parent.height)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter

            MouseArea {
                property real px: 0
                property real py: 0

                function reset_pos(x, y) {
                    px = x
                    py = y
                }

                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                onPressed: e => reset_pos(e.x, e.y)
                onPositionChanged: e => {
                    if (e.buttons & (Qt.RightButton | Qt.MiddleButton)) {
                        pan((e.x - px) / width * 2.0, (py - e.y) / height * 2.0)
                        reset_pos(e.x, e.y)
                    } else {
                        mouseDown(sat(e.x / width), sat(e.y / height))
                    }
                }
                onWheel: e => zoom(e.angleDelta.y, (e.x / width) * 2.0 - 1.0, 1.0 - (e.y / height) * 2.0)
            }
        }
    }

    T.Slider {
        id: seek_bar
        from: 0
        to: duration * timebase
        value: frame_ts
        onMoved: update_time(value)
        height: 20
        Layout.fillWidth: true

        background: Rectangle {
            color: "gray"
            anchors.fill: parent

            Rectangle {
                width: seek_bar.visualPosition * parent.width
                height: parent.height
                color: "orange"
            }
        }
    }

    Action {
        id: play_action
        checkable: true
        text: "▶"
        onTriggered: timer.running ? stop_timer() : start_timer()
    }

    Action {
        id: prevframe_action
        text: "<"
        onTriggered: set_frame_index(clamp(frame_index - 1, 0, duration_i))
    }

    Action {
        id: nextframe_action
        text: ">"
        onTriggered: set_frame_index(clamp(frame_index + 1, 0, duration_i))
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignCenter

        Button { action: prevframe_action }
        Button { action: play_action }
        Button { action: nextframe_action }

        Label {
            text: timed_text()
        }
    }

    Timer {
        id: timer
        running: false
        repeat: true
        interval: 1
        onTriggered: update_time(-1)
    }
}
