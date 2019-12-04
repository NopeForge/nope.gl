#
# Copyright 2019 GoPro Inc.
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

test-api-backend: FUNC_NAME = test_backend
API_TESTS += test-api-backend

test-api-reconfigure: FUNC_NAME = test_reconfigure
API_TESTS += test-api-reconfigure

test-api-reconfigure-fail: FUNC_NAME = test_reconfigure_fail
API_TESTS += test-api-reconfigure-fail

test-api-ctx-ownership: FUNC_NAME = test_ctx_ownership
API_TESTS += test-api-ctx-ownership

test-api-ctx-ownership-subgraph: FUNC_NAME = test_ctx_ownership_subgraph
API_TESTS += test-api-ctx-ownership-subgraph

test-api-capture-buffer-lifetime: FUNC_NAME = test_capture_buffer_lifetime
API_TESTS += test-api-capture-buffer-lifetime

$(API_TESTS):
	@echo $@
	@$(PYTHON) -c 'from api import $(FUNC_NAME); $(FUNC_NAME)()' > /dev/null

test-api: $(API_TESTS)

TESTS += $(API_TESTS)
