#
# Copyright 2020-2022 GoPro Inc.
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

import os
import os.path as op
import sys

from pynopegl_utils.module import load_script


def _run_test(func_name, tester, ref_data, out_data):
    err = []
    if len(ref_data) != len(out_data):
        err = [f"{func_name}: data len mismatch (ref:{len(ref_data)} out:{len(out_data)})"]
    err += tester.compare_data(func_name, ref_data, out_data)
    return err


def _set_ref_data(tester, ref_filepath, data):
    with open(ref_filepath, "w") as ref_file:
        ref_file.write(tester.serialize(data))


def _get_ref_data(tester, ref_filepath):
    with open(ref_filepath) as ref_file:
        serialized_data = ref_file.read()
    return tester.deserialize(serialized_data)


def _run_test_default(func_name, tester, ref_filepath, dump=False):
    if not op.exists(ref_filepath):
        sys.stderr.write(f"{func_name}: reference file {ref_filepath} not found, use REFGEN=create to create it\n")
        sys.exit(1)
    ref_data = _get_ref_data(tester, ref_filepath)
    out_data = tester.get_out_data(dump, func_name)
    return _run_test(func_name, tester, ref_data, out_data)


def _run_test_gen_create(func_name, tester, ref_filepath, dump=False):
    out_data = tester.get_out_data(dump, func_name)
    if not op.exists(ref_filepath):
        sys.stderr.write(f"{func_name}: creating {ref_filepath}\n")
        _set_ref_data(tester, ref_filepath, out_data)
        return None
    ref_data = _get_ref_data(tester, ref_filepath)
    return _run_test(func_name, tester, ref_data, out_data)


def _run_test_gen_update(func_name, tester, ref_filepath, dump=False):
    out_data = tester.get_out_data(dump, func_name)
    if not op.exists(ref_filepath):
        sys.stderr.write(f"{func_name}: creating {ref_filepath}\n")
        _set_ref_data(tester, ref_filepath, out_data)
        return None
    ref_data = _get_ref_data(tester, ref_filepath)
    err = _run_test(func_name, tester, ref_data, out_data)
    if err:
        sys.stderr.write(f"{func_name}: re-generating {ref_filepath}\n")
        _set_ref_data(tester, ref_filepath, out_data)
    return None


def _run_test_gen_force(func_name, tester, ref_filepath, dump=False):
    if not op.exists(ref_filepath):
        sys.stderr.write(f"{func_name}: creating {ref_filepath}\n")
    else:
        sys.stderr.write(f"{func_name}: re-generating {ref_filepath}\n")
    out_data = tester.get_out_data(dump, func_name)
    _set_ref_data(tester, ref_filepath, out_data)
    return []


_refgen_map = {
    "no": _run_test_default,
    "create": _run_test_gen_create,
    "update": _run_test_gen_update,
    "force": _run_test_gen_force,
}


def run():
    refgen_opt = os.environ.get("REFGEN", "no")

    allowed_gen_opt = _refgen_map.keys()
    if refgen_opt not in allowed_gen_opt:
        allowed_str = ", ".join(allowed_gen_opt)
        sys.stderr.write(f"REFGEN environment variable must be any of {allowed_str}\n")
        sys.exit(1)

    tests_opts = os.environ.get("TESTS_OPTIONS")

    allowed_tests_opts = ("dump",)
    if tests_opts is not None and tests_opts not in allowed_tests_opts:
        allowed_str = ", ".join(allowed_tests_opts)
        sys.stderr.write(f"TESTS_OPTIONS environment variable must be any of {allowed_str}\n")
        sys.exit(1)
    dump = tests_opts == "dump"

    if len(sys.argv) not in (3, 4):
        sys.stderr.write(
            "Usage: [TESTS_OPTIONS={} REFGEN={}] {} <script_path> <func_name> [<ref_filepath>]\n".format(
                "|".join(allowed_tests_opts), "|".join(allowed_gen_opt), op.basename(sys.argv[0])
            )
        )
        sys.exit(1)

    if len(sys.argv) == 3:
        script_path, func_name, ref_filepath = sys.argv[1], sys.argv[2], None
    else:
        script_path, func_name, ref_filepath = sys.argv[1:4]
    module = load_script(script_path)
    func = getattr(module, func_name)

    # Ensure PySide/Qt is not imported
    assert not any(k.startswith(("PySide", "Qt")) for k in globals().keys())

    if ref_filepath is None:
        ret = func()
        if ret:
            print(ret)
        sys.exit(0)

    tester = func.tester
    test_func = _refgen_map[refgen_opt]
    err = test_func(func_name, tester, ref_filepath, dump)
    if err:
        sys.stderr.write(f"{func_name} failed\n")
        sys.stderr.write("\n".join(err) + "\n")
        sys.exit(1)
    print(f"{func_name} passed")
    sys.exit(0)
