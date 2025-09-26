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

package test;

import com.caucho.hessian.io.Hessian2Output;
import com.caucho.hessian.io.Hessian2Input;
import com.caucho.hessian.test.TestHessian2Servlet;

import java.lang.reflect.Method;

public class Hessian {
    public static void main(String[] args) throws Exception {
        if (args.length > 1) {
            testCustomClassMethod(args[0], args[1]);
            return;
        }

        if (args[0].startsWith("reply")) {
            Method method = TestHessian2Servlet.class.getMethod(args[0]);
            TestHessian2Servlet servlet = new TestHessian2Servlet();
            Object object = method.invoke(servlet);

            Hessian2Output output = new Hessian2Output(System.out);
            output.writeObject(object);
            output.flush();
        } else if (args[0].startsWith("customReply")) {
            Method method = TestCustomReply.class.getMethod(args[0]);
            TestCustomReply testCustomReply = new TestCustomReply(System.out);
            method.invoke(testCustomReply);
        } else if (args[0].startsWith("arg")) {
            Hessian2Input input = new Hessian2Input(System.in);
            Object o = input.readObject();

            Method method = TestHessian2Servlet.class.getMethod(args[0], Object.class);
            TestHessian2Servlet servlet = new TestHessian2Servlet();
            System.out.print(method.invoke(servlet, o));
        } else if (args[0].startsWith("customArg")) {
            Method method = TestCustomDecode.class.getMethod(args[0]);
            TestCustomDecode testCustomDecode = new TestCustomDecode(System.in);
            System.out.print(method.invoke(testCustomDecode));
        } else if (args[0].startsWith("throw_")) {
            Method method = method = TestThrowable.class.getMethod(args[0]);
            TestHessian2Servlet servlet = new TestHessian2Servlet();
            Object object = method.invoke(servlet);

            Hessian2Output output = new Hessian2Output(System.out);
            output.writeObject(object);
            output.flush();
        } else if (args[0].startsWith("java8_")) {
            // add java8 java.time Object test
            Method method = TestJava8Time.class.getMethod(args[0]);
            Object obj = new Object();
            Object object = method.invoke(obj);

            Hessian2Output output = new Hessian2Output(System.out);
            output.writeObject(object);
            output.flush();
        } else if (args[0].startsWith("javaSql_")) {
            if (args[0].startsWith("javaSql_encode")) {

                Hessian2Input input = new Hessian2Input(System.in);
                Object o = input.readObject();

                Method method = TestJavaSqlTime.class.getMethod(args[0], Object.class);
                TestJavaSqlTime testJavaSqlTime = new TestJavaSqlTime();
                System.out.print(method.invoke(testJavaSqlTime, o));
            } else {
                Method method = TestJavaSqlTime.class.getMethod(args[0]);
                TestJavaSqlTime testJavaSqlTime = new TestJavaSqlTime();
                Object object = method.invoke(testJavaSqlTime);

                Hessian2Output output = new Hessian2Output(System.out);
                output.writeObject(object);
                output.flush();
            }

        }
    }

    private static void testCustomClassMethod(String methodName, String className) throws Exception {
        Class<?> clazz = Class.forName(className);
        Method method = clazz.getMethod(methodName);
        Object target = clazz.newInstance();
        Object result = method.invoke(target);
        Hessian2Output output = new Hessian2Output(System.out);
        output.writeObject(result);
        output.flush();
    }
}
