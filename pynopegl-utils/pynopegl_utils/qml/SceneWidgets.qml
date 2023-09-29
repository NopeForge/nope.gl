import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.qmlmodels // DelegateChooser is experimental

GroupBox {
    required property string box_title
    required property var window
    property alias widget_model: paramList.model

    id: root
    title: box_title
    Layout.fillWidth: true
    Layout.margins: 5
    visible: paramList.count > 0

    /*
     * A scene widget is composed of a Label for the 1st column and the
     * specified widget for the 2nd column.
     */
    component SceneWidget: Item {
        id: sceneWidget
        property string label
        property int row
        default property Item contentItem

        Binding {
            target: sceneWidget.contentItem
            property: "parent"
            value: paramGrid
        }

        Binding {
            target: sceneWidget.Layout
            property: "row"
            value: sceneWidget.row
        }

        Binding {
            target: sceneWidget.Layout
            property: "column"
            value: 1
        }

        Label {
            text: label + ":"
            parent: paramGrid
            Layout.column: 0
            Layout.row: sceneWidget.row
        }
    }

    contentItem: GridLayout {
        id: paramGrid
        Instantiator {
            id: paramList

            delegate: DelegateChooser {
                role: "type"

                DelegateChoice {
                    roleValue: "range"
                    SceneWidget {
                        label: model.label
                        row: index
                        property bool fixed: model.step == 1.0
                        RowLayout {
                            Slider {
                                Layout.fillWidth: true
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
                }

                DelegateChoice {
                    roleValue: "vector"
                    SceneWidget {
                        label: model.label
                        row: index
                        property var mdl: model
                        RowLayout {
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
                                        return decimal * decimalFactor;
                                    }

                                    validator: DoubleValidator {
                                        bottom: Math.min(spinBox.from, spinBox.to)
                                        top:  Math.max(spinBox.from, spinBox.to)
                                        decimals: spinBox.decimals
                                        notation: DoubleValidator.StandardNotation
                                    }

                                    textFromValue: function(value, locale) {
                                        return Number(value / decimalFactor).toLocaleString(locale, 'f', spinBox.decimals);
                                    }

                                    valueFromText: function(text, locale) {
                                        return Math.round(Number.fromLocaleString(locale, text) * decimalFactor);
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
                }

                DelegateChoice {
                    roleValue: "color"
                    SceneWidget {
                        label: model.label
                        row: index
                        Button {
                            ColorDialog {
                                id: color_dialog
                                parentWindow: root.window
                                onSelectedColorChanged: model.val = selectedColor
                                Component.onCompleted: selectedColor = model.val
                                options: ColorDialog.NoButtons
                            }
                            palette.button : color_dialog.selectedColor
                            text: color_dialog.selectedColor
                            onClicked: color_dialog.open()
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "bool"
                    SceneWidget {
                        label: model.label
                        row: index
                        Switch {
                            checked: model.val
                            onToggled: model.val = checked
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "file"
                    SceneWidget {
                        label: model.label
                        row: index
                        RowLayout {
                            TextField {
                                id: file_text
                                text: model.val ? model.val : ""
                                Layout.fillWidth: true
                                onEditingFinished: model.val = text
                            }
                            Button {
                                FileDialog {
                                    id: file_dialog
                                    parentWindow: root.window
                                    nameFilters: [model.filter]
                                    onAccepted: model.val = selectedFile.toString()
                                }
                                text: "Open"
                                background.implicitWidth: 0
                                onClicked: file_dialog.open()
                            }
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "list"
                    SceneWidget {
                        label: model.label
                        row: index
                        // this is needed to avoid confusion between
                        // the element "model" entry and the ComboBox.model.
                        // That's also why we don't use model.val here.
                        property var _choices: model.choices
                        ComboBox {
                            Layout.fillWidth: true
                            model: _choices
                            currentIndex: _choices.indexOf(val)
                            onActivated: val = _choices[currentIndex]
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "text"
                    SceneWidget {
                        label: model.label
                        row: index
                        TextField {
                            Layout.fillWidth: true
                            text: model.val
                            onEditingFinished: model.val = text
                        }
                    }
                }
            }
        }
    }
}

