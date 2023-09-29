#
# Copyright 2023 Clément Bœsch <u pkh.me>
# Copyright 2023 Nope Foundry
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

from PySide6.QtCore import QAbstractListModel, QModelIndex, Qt, Slot


class Model(QAbstractListModel):
    """Model for both the build scene options widgets and live controls"""

    _roles = ["type", "label", "val", "min", "max", "n", "step", "choices", "filter"]
    _roles_map = {Qt.UserRole + i: s for i, s in enumerate(_roles)}

    def __init__(self, parent=None):
        super().__init__(parent)
        self._data = []

    @Slot(object)
    def reset_data_model(self, data):
        self.beginResetModel()
        self._data = data
        self.endResetModel()

    def get_row(self, row):
        return self._data[row]

    def roleNames(self):
        names = super().roleNames()
        names.update({k: v.encode() for k, v in self._roles_map.items()})
        return names

    def rowCount(self, parent=QModelIndex()):
        return len(self._data)

    def data(self, index, role: int):
        if not index.isValid():
            return None
        return self._data[index.row()].get(self._roles_map[role])

    def setData(self, index, value, role):
        if not index.isValid():
            return False
        item = self._data[index.row()]
        item[self._roles_map[role]] = value
        self.dataChanged.emit(index, index)
        return True
