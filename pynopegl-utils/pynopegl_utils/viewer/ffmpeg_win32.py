#
# Copyright 2023 Matthieu Bouron <matthieu.bouron@gmail.com>
# Copyright 2023 Nope Forge
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

import platform

if platform.system() != "Windows":
    raise NotImplementedError

import msvcrt
import os
import shutil
import subprocess
import sys


def run(args):
    args = args.copy()
    for index, arg in enumerate(args):
        if arg.startswith("handle:"):
            handle = int(arg[len("handle:") :])
            fd = msvcrt.open_osfhandle(handle, os.O_RDONLY)
            args[index] = f"pipe:{fd}"
        else:
            args[index] = subprocess.list2cmdline([arg])

    ffmpeg = shutil.which("ffmpeg.exe")
    sys.exit(os.spawnl(os.P_WAIT, ffmpeg, "ffmpeg.exe", *args))


if __name__ == "__main__":
    run(sys.argv[1:])
