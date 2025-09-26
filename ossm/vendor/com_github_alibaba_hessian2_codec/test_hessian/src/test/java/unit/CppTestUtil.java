/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package unit;

import com.alibaba.com.caucho.hessian.io.Hessian2Input;
import junit.framework.Assert;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;

/**
 * @author wongoo
 */
public class CppTestUtil {

    public static Object readCppObject(String func) {
        System.out.println("read cpp data: " + func);
        try {
            Process process = Runtime.getRuntime()
                    .exec("bazel test //hessian2/testcase:" + func,
                            null,
                            new File(".."));

            int exitValue = process.waitFor();
            if (exitValue != 0) {
                Assert.fail(readString(process.getErrorStream()));
                return null;
            }

            InputStream is = process.getInputStream();
            Hessian2Input input = new Hessian2Input(is);
            return input.readObject();
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    private static String readString(InputStream in) throws IOException {
        StringBuilder out = new StringBuilder();
        InputStreamReader reader = new InputStreamReader(in, StandardCharsets.UTF_8);
        char[] buffer = new char[4096];

        int bytesRead;
        while ((bytesRead = reader.read(buffer)) != -1) {
            out.append(buffer, 0, bytesRead);
        }

        return out.toString();
    }
}
