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
import QtQuick.Layouts

Popup {
    modal: true
    padding: 20
    anchors.centerIn: parent
    implicitWidth: 640
    implicitHeight: 480

    contentItem: ColumnLayout {
        spacing: 20
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: aboutContent
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
        }
        Button {
            text: "OK"
            onClicked: aboutPopup.visible = false
            Layout.alignment: Qt.AlignHCenter
        }
    }

    function set_about_content(text) { aboutContent.text = text; }
}
