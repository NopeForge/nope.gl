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

from .cmp import CompareBase, get_test_decorator


class _CompareFloats(CompareBase):
    def __init__(self, func, tolerance=0.001, **func_kwargs):
        self._func = func
        self._func_kwargs = func_kwargs
        self._tolerance = tolerance

    @staticmethod
    def serialize(data):
        ret = ""
        for name, floats in data:
            ret += "{}: {}\n".format(name, " ".join("%f" % f for f in floats))
        return ret

    @staticmethod
    def deserialize(data):
        ret = []
        for line in data.splitlines():
            name, floats = line.split(":", 1)
            ret.append((name, [float(f) for f in floats.split()]))
        return ret

    def get_out_data(self, dump=False, func_name=None):
        return self._func(**self._func_kwargs)

    def compare_data(self, test_name, ref_data, out_data):
        err = []
        for float_set_id, (ref, out) in enumerate(zip(ref_data, out_data)):
            ref_name, ref_floats = ref
            out_name, out_floats = out
            if ref_name != out_name:
                err.append(
                    f"{test_name} float sets {float_set_id} have different names: " f'"{ref_name}" vs "{out_name}"'
                )
                break
            if len(ref_floats) != len(out_floats):
                err.append(
                    f"{test_name}: number of floats is different " f"(ref:{len(ref_floats)}, out:{len(out_floats)})"
                )
                break
            for i, (f0, f1) in enumerate(zip(ref_floats, out_floats)):
                diff = abs(f0 - f1)
                if diff > self._tolerance:
                    err.append(
                        f"{test_name} diff too large for float {ref_name}[{i}]: "
                        f"|{f0}-{f1}|={diff} (>{self._tolerance})"
                    )
        return err


test_floats = get_test_decorator(_CompareFloats)
