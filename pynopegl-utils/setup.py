#
# Copyright 2016-2022 GoPro Inc.
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

from setuptools import find_packages, setup

setup(
    name="pynopegl-utils",
    version="0.1",
    packages=find_packages(),
    install_requires=["pynopegl"],
    entry_points={
        "console_scripts": [
            "ngl-control = pynopegl_utils.controller:run",
            "ngl-test = pynopegl_utils.tests:run",
            "ngl-viewer = pynopegl_utils.viewer:run",
            "ngl-diff = pynopegl_utils.diff:run",
        ],
    },
    package_data={
        "": [
            "assets/*",
            "diff/shaders/*",
            "examples/data/*",
            "examples/shaders/*.frag",
            "examples/shaders/*.comp",
            "examples/shaders/*.vert",
            "hooks/desktop.py",
            "qml/*.qml",
        ],
    },
)
