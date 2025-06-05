# Copyright 2022 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sphinx  # noqa
from flask import Flask, jsonify
from random_number_generator import generate_random_number

app = Flask(__name__)


@app.route("/random-number", methods=["GET"])
def get_random_number():
    return jsonify({"number": generate_random_number.generate_random_number()})


def main():
    """Start the python web server"""
    app.run()
