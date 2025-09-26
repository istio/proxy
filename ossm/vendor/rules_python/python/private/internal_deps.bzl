#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"Python toolchain module extension for internal rule use"

load("@bazel_skylib//lib:modules.bzl", "modules")
load("//python/private/pypi:deps.bzl", "pypi_deps")
load(":internal_config_repo.bzl", "internal_config_repo")

def _internal_deps():
    internal_config_repo(name = "rules_python_internal")
    pypi_deps()

internal_deps = modules.as_extension(
    _internal_deps,
    doc = "This extension registers internal rules_python dependencies.",
)
