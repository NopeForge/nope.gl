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

from typing import Optional, Sequence

from PIL import Image

from .cmp import CompareBase, CompareSceneBase, get_test_decorator

_HSIZE = 8
_HNBITS = _HSIZE * _HSIZE * 2
_MODE = "RGBA"


class _CompareFingerprints(CompareSceneBase):
    def __init__(self, scene_func, tolerance: int = 0, **kwargs):
        super().__init__(scene_func, **kwargs)
        self._tolerance = tolerance

    @staticmethod
    def serialize(data: Sequence[Sequence[int]]) -> str:
        ret = ""
        for comp_hashes in data:
            ret += " ".join(format(x, "032X") for x in comp_hashes) + "\n"
        return ret

    @staticmethod
    def deserialize(data: str) -> Sequence[Sequence[int]]:
        ret = []
        for line in data.splitlines():
            hashes = [int(x, 16) for x in line.split()]
            ret.append(hashes)
        return ret

    @staticmethod
    def _get_plane_hashes(buf) -> Sequence[int]:
        hashes = []
        linesize = _HSIZE + 1
        comp_bufs = (buf[x::4] for x in range(4))  # R, G, B, A
        for comp_buf in comp_bufs:
            comp_hash = 0
            for y in range(_HSIZE):
                for x in range(_HSIZE):
                    pos = y * linesize + x
                    px_ref = comp_buf[pos]
                    px_xp1 = comp_buf[pos + 1]
                    px_yp1 = comp_buf[pos + linesize]
                    h_bit = px_ref < px_xp1
                    v_bit = px_ref < px_yp1
                    comp_hash = comp_hash << 2 | h_bit << 1 | v_bit
            hashes.append(comp_hash)
        return hashes

    def get_out_data(self, dump: bool = False, func_name: Optional[str] = None) -> Sequence[Sequence[int]]:
        hashes = []
        dump_index = 0
        for width, height, capture_buffer in self.render_frames():
            img = Image.frombytes(_MODE, (width, height), capture_buffer, "raw", _MODE, 0, 1)
            if dump:
                CompareBase.dump_image(img, dump_index, func_name)
                dump_index += 1
            img = img.resize((_HSIZE + 1, _HSIZE + 1), resample=Image.Resampling.LANCZOS)
            data = img.tobytes()
            frame_hashes = self._get_plane_hashes(data)
            hashes.append(frame_hashes)
        return hashes

    @staticmethod
    def _hash_repr(hash_val: int) -> str:
        linesize = _HSIZE + 1
        diff_chars = ".v>+"  # identical, vertical diff, horizontal diff, vertical+horizontal diff
        ret = ""
        for y in range(_HSIZE):
            line = ""
            for x in range(_HSIZE):
                pos = y * linesize + x
                bits = hash_val >> (pos * 2) & 0b11
                line += " {}".format(diff_chars[bits])
            ret += line + "\n"
        return ret

    def compare_data(self, test_name: str, ref_data, out_data) -> Sequence[str]:
        err = []
        for frame, (frame_ref_hashes, frame_out_hashes) in enumerate(zip(ref_data, out_data)):
            for comp, (ref_hash, out_hash) in enumerate(zip(frame_ref_hashes, frame_out_hashes)):
                hash_diff = ref_hash ^ out_hash
                bstring = f"{hash_diff:b}"
                diff = bstring.count("1") * 100 // _HNBITS
                if diff > self._tolerance:
                    hash_repr = self._hash_repr(hash_diff)
                    err.append(
                        f"{test_name} frame #{frame} Component {_MODE[comp]}: "
                        f"Diff too high ({diff}% > {self._tolerance}%)\n"
                        f"{hash_repr:s}"
                    )

        return err


test_fingerprint = get_test_decorator(_CompareFingerprints)
