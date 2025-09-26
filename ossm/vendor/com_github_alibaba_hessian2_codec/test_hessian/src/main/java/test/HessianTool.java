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

import com.caucho.hessian.io.Hessian2Input;
import com.caucho.hessian.io.Hessian2Output;
import com.caucho.hessian.io.SerializerFactory;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

public class HessianTool {
    private static SerializerFactory serializerFactory = new SerializerFactory();

    public static byte[] serialize(Object obj) {
        ByteArrayOutputStream ops = new ByteArrayOutputStream();
        Hessian2Output out = new Hessian2Output(ops);
        out.setSerializerFactory(serializerFactory);

        try {
            out.writeObject(obj);
            out.close();
        } catch (IOException e) {
            throw new RuntimeException("hessian error", e);
        }

        byte[] bytes = ops.toByteArray();
        return bytes;
    }

    public static Object deserialize(byte[] bytes) {
        ByteArrayInputStream ins = new ByteArrayInputStream(bytes);
        Hessian2Input in = new Hessian2Input(ins);
        in.setSerializerFactory(serializerFactory);

        try {
            Object o = in.readObject();
            in.close();
            return o;
        } catch (IOException e) {
            throw new RuntimeException("hessian error", e);
        }
    }

}
