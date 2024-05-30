# Copyright(C) 2023 InfiniFlow, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# import importlib.metadata
#
# __version__ = importlib.metadata.version("infinity_sdk")

import pkg_resources
__version__ = pkg_resources.get_distribution("infinity_sdk").version

from infinity.common import URI, NetworkAddress, LOCAL_HOST, EMBEDDED_HOST
from infinity.infinity import InfinityConnection
from infinity.remote_thrift.infinity import RemoteThriftInfinityConnection
from infinity.embedded_infinity.infinity import EmbeddedInfinityConnection

def connect(
        uri: URI = LOCAL_HOST
) -> InfinityConnection:
    if isinstance(uri, NetworkAddress) and (uri.port == 9090 or uri.port == 23817 or uri.port == 9070):
        return RemoteThriftInfinityConnection(uri)
    elif isinstance(uri, NetworkAddress) and uri == EMBEDDED_HOST:
        return EmbeddedInfinityConnection(uri)
    else:
        raise Exception(f"unknown uri: {uri}")
