// Licensed to the Software Freedom Conservancy (SFC) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The SFC licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package com.github.bazelbuild.rules_jvm_external.zip;

import java.time.LocalDateTime;
import java.time.ZoneId;
import java.util.zip.ZipEntry;

public class StableZipEntry extends ZipEntry {

  // File time is taken from the epoch (1970-01-01T00:00:00Z), but zip files
  // have a different epoch. Oof. Make something sensible up.
  private static final LocalDateTime DOS_EPOCH = LocalDateTime.of(2010, 1, 1, 0, 0);
  // ZIP timestamps have a resolution of 2 seconds.
  // see http://www.info-zip.org/FAQ.html#limits
  private static final long MINIMUM_TIMESTAMP_INCREMENT = 2000L;

  public StableZipEntry(String name) {
    super(name);

    long timestamp = DOS_EPOCH.atZone(ZoneId.systemDefault()).toInstant().toEpochMilli();
    // Returns the normalized timestamp for a jar entry based on its name. This is necessary since
    // javac will, when loading a class X, prefer a source file to a class file, if both files have
    // the same timestamp. Therefore, we need to adjust the timestamp for class files to slightly
    // after the normalized time.
    // https://github.com/bazelbuild/bazel/blob/master/src/java_tools/buildjar/java/com/google/devtools/build/buildjar/jarhelper/JarHelper.java#L124
    timestamp = name.endsWith(".class") ? timestamp + MINIMUM_TIMESTAMP_INCREMENT : timestamp;

    // We only set the MS-DOS time and not the extended timestamp so the resulting Zip file doesn't
    // encode the timezone. The method uses the system timezone internally for conversion.
    setTime(timestamp);
  }
}
