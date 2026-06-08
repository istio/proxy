// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.example;

import com.code_intelligence.jazzer.api.FuzzedDataProvider;
import com.google.devtools.build.runfiles.Runfiles;

import java.io.IOException;
import java.io.File;

public class NativeRunfileFuzzTest {

    static {
        System.loadLibrary("native_runfile");
    }

    public static void fuzzerTestOneInput(FuzzedDataProvider data) throws IOException {
        if (data.consumeBoolean()) {
            loadJavaRunfile();
        } else {
            loadCppRunfile();
        }
    }

    private static void loadJavaRunfile() throws IOException {
        Runfiles runfiles = Runfiles.create();
        String path = runfiles.rlocation("rules_fuzzing/examples/java/corpus_0.txt");
        File runfile = new File(path);
        if (!runfile.exists()) {
            throw new IOException("Java runfile not found");
        }
    }

    private static native void loadCppRunfile();
}
