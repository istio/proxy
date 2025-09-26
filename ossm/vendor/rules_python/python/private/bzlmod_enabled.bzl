#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Variable to check if bzlmod is enabled"""

# When bzlmod is enabled, canonical repos names have @@ in them, while under
# workspace builds, there is never a @@ in labels.
BZLMOD_ENABLED = "@@" in str(Label("//:unused"))
