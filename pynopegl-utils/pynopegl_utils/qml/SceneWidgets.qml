import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.qmlmodels // DelegateChooser is experimental

GroupBox {
    required property string box_title
    property alias widget_model: paramList.model

    title: box_title
    Layout.fillWidth: true
    Layout.margins: 5
    visible: paramList.count > 0

    contentItem: GridLayout {
        id: paramGrid
        Instantiator {
            id: paramList

            delegate: DelegateChooser {
                role: "type"

                DelegateChoice {
                    roleValue: "range"
                    QtObject {
                        property bool fixed: model.step == 1.0
                        property Label col0: Label {
                            parent: paramGrid
                            Layout.column: 0
                            Layout.row: index
                            text: model.label + ":"
                        }
                        property RowLayout col1: RowLayout {
                            parent: paramGrid
                            Layout.column: 1
                            Layout.row: index
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
                    QtObject {
                        property Label col0: Label {
                            parent: paramGrid
                            Layout.column: 0
                            Layout.row: index
                            text: model.label + ":"
                        }
                        property var mdl: model
                        property RowLayout col1: RowLayout {
                            parent: paramGrid
                            Layout.column: 1
                            Layout.row: index

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
                    QtObject {
                        property Label col0: Label {
                            parent: paramGrid
                            Layout.column: 0
                            Layout.row: index
                            text: model.label + ":"
                        }
                        property Button col1: Button {
                            ColorDialog {
                                id: color_dialog
                                onSelectedColorChanged: model.val = selectedColor
                                Component.onCompleted: selectedColor = model.val
                                options: ColorDialog.NoButtons
                            }
                            parent: paramGrid
                            Layout.column: 1
                            Layout.row: index
                            palette.button : color_dialog.selectedColor
                            text: color_dialog.selectedColor
                            onClicked: color_dialog.open()
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "bool"
                    QtObject {
                        property Label col0: Label {
                            parent: paramGrid
                            Layout.column: 0
                            Layout.row: index
                            text: model.label + ":"
                        }
                        property Switch col1: Switch {
                            parent: paramGrid
                            Layout.column: 1
                            Layout.row: index
                            checked: model.val
                            onToggled: model.val = checked
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "file"
                    RowLayout {
                        property Label col0: Label {
                            parent: paramGrid
                            Layout.column: 0
                            Layout.row: index
                            text: model.label + ":"
                        }
                        property RowLayout col1: RowLayout {
                            parent: paramGrid
                            Layout.column: 1
                            Layout.row: index
                            TextField {
                                id: file_text
                                text: model.val ? model.val : ""
                                Layout.fillWidth: true
                                onEditingFinished: model.val = text
                            }
                            Button {
                                FileDialog {
                                    id: file_dialog
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
                    QtObject {
                        // this is needed to avoid confusion between
                        // the element "model" entry and the ComboBox.model.
                        // That's also why we don't use model.val here.
                        property var _choices: model.choices
                        property Label col0: Label {
                            parent: paramGrid
                            Layout.column: 0
                            Layout.row: index
                            text: model.label + ":"
                        }
                        property ComboBox col1: ComboBox {
                            parent: paramGrid
                            Layout.column: 1
                            Layout.row: index
                            Layout.fillWidth: true
                            model: _choices
                            currentIndex: _choices.indexOf(val)
                            onActivated: val = _choices[currentIndex]
                        }
                    }
                }

                DelegateChoice {
                    roleValue: "text"
                    QtObject {
                        property Label col0: Label {
                            parent: paramGrid
                            Layout.column: 0
                            Layout.row: index
                            text: model.label + ":"
                        }
                        property TextField col1: TextField {
                            parent: paramGrid
                            Layout.column: 1
                            Layout.row: index
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

