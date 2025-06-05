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

import com.alibaba.com.caucho.hessian.io.Hessian2Output;
import com.alibaba.fastjson.JSON;
import com.alibaba.fastjson.JSONObject;
import com.caucho.hessian.test.A0;
import com.caucho.hessian.test.A1;
import test.generic.BusinessData;
import test.generic.Response;
import test.model.DateDemo;

import java.io.OutputStream;
import java.io.Serializable;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class TestCustomReply {

    private Hessian2Output output;
    private HashMap<Class<?>, String> typeMap;

    public TestCustomReply(OutputStream os) {
        output = new Hessian2Output(os);

        typeMap = new HashMap<>();
        typeMap.put(Void.TYPE, "void");
        typeMap.put(Boolean.class, "boolean");
        typeMap.put(Byte.class, "byte");
        typeMap.put(Short.class, "short");
        typeMap.put(Integer.class, "int");
        typeMap.put(Long.class, "long");
        typeMap.put(Float.class, "float");
        typeMap.put(Double.class, "double");
        typeMap.put(Character.class, "char");
        typeMap.put(String.class, "string");
        typeMap.put(StringBuilder.class, "string");
        typeMap.put(Object.class, "object");
        typeMap.put(Date.class, "date");
        typeMap.put(Boolean.TYPE, "boolean");
        typeMap.put(Byte.TYPE, "byte");
        typeMap.put(Short.TYPE, "short");
        typeMap.put(Integer.TYPE, "int");
        typeMap.put(Long.TYPE, "long");
        typeMap.put(Float.TYPE, "float");
        typeMap.put(Double.TYPE, "double");
        typeMap.put(Character.TYPE, "char");
        typeMap.put(boolean[].class, "[boolean");
        typeMap.put(byte[].class, "[byte");
        typeMap.put(short[].class, "[short");
        typeMap.put(int[].class, "[int");
        typeMap.put(long[].class, "[long");
        typeMap.put(float[].class, "[float");
        typeMap.put(double[].class, "[double");
        typeMap.put(char[].class, "[char");
        typeMap.put(String[].class, "[string");
        typeMap.put(Object[].class, "[object");
        typeMap.put(Date[].class, "[date");
    }

    public void customReplyTypedFixedListHasNull() throws Exception {
        Object[] o = new Object[] { new A0(), new A1(), null };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableListHasNull() throws Exception {
        Object[] o = new Object[] { new A0(), new A1(), null };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyUntypedFixedListHasNull() throws Exception {
        Object[] o = new Object[] { new A0(), new A1(), null };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(o.length, null);
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyUntypedVariableListHasNull() throws Exception {
        Object[] o = new Object[] { new A0(), new A1(), null };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, null);
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_A0() throws Exception {
        A0[] o = new A0[] { new A0(), new A0(), null };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableList_A0() throws Exception {
        A0[] o = new A0[] { new A0(), new A0(), null };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, "[com.caucho.hessian.test.A0");
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_int() throws Exception {
        int[] o = new int[] { 1, 2, 3 };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableList_int() throws Exception {
        int[] o = new int[] { 1, 2, 3 };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_long() throws Exception {
        long[] o = new long[] { 1, 2, 3 };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableList_long() throws Exception {
        long[] o = new long[] { 1, 2, 3 };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_float() throws Exception {
        float[] o = new float[] { 1, 2, 3 };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableList_float() throws Exception {
        float[] o = new float[] { 1, 2, 3 };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_double() throws Exception {
        double[] o = new double[] { 1, 2, 3 };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableList_double() throws Exception {
        double[] o = new double[] { 1, 2, 3 };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_short() throws Exception {
        short[] o = new short[] { 1, 2, 3 };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableList_short() throws Exception {
        short[] o = new short[] { 1, 2, 3 };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_char() throws Exception {
        char[] o = new char[] { '1', '2', '3' };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(o.length, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedVariableList_char() throws Exception {
        char[] o = new char[] { '1', '2', '3' };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_boolean() throws Exception {
        boolean[] o = new boolean[] { true, false, true };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableList_boolean() throws Exception {
        boolean[] o = new boolean[] { true, false, true };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_date() throws Exception {
        Date[] o = new Date[] { new Date(1560864000), new Date(1560864000), new Date(1560864000) };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedVariableList_date() throws Exception {
        Date[] o = new Date[] { new Date(1560864000), new Date(1560864000), new Date(1560864000) };
        if (output.addRef(o)) {
            return;
        }
        boolean hasEnd = output.writeListBegin(-1, typeMap.get(o.getClass()));
        for (Object tmp : o) {
            output.writeObject(tmp);
        }
        if (hasEnd) {
            output.writeListEnd();
        }
        output.flush();
    }

    public void customReplyTypedFixedList_arrays() throws Exception {
        int[][][] o = new int[][][] { { { 1, 2, 3 }, { 4, 5, 6, 7 } }, { { 8, 9, 10 }, { 11, 12, 13, 14 } } };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedFixedList_A0arrays() throws Exception {
        A0[][][] o = new A0[][][] { { { new A0(), new A0(), new A0() }, { new A0(), new A0(), new A0(), null } },
                { { new A0() }, { new A0() } } };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedFixedList_Test() throws Exception {
        TypedListTest o = new TypedListTest();
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedFixedList_Object() throws Exception {
        Object[] o = new Object[] { new A0() };
        output.writeObject(o);
        output.flush();
    }

    public void customReplyTypedFixedInteger() throws Exception {
        BigInteger integer = new BigInteger("4294967298");
        output.writeObject(integer);
        output.flush();
    }

    public void customReplyTypedFixedList_BigInteger() throws Exception {
        BigInteger[] integers = new BigInteger[] {
                new BigInteger("1234"),
                new BigInteger("12347890"),
                new BigInteger("123478901234"),
                new BigInteger("1234789012345678"),
                new BigInteger("123478901234567890"),
                new BigInteger("1234789012345678901234"),
                new BigInteger("12347890123456789012345678"),
                new BigInteger("123478901234567890123456781234"),
                new BigInteger("1234789012345678901234567812345678"),
                new BigInteger("12347890123456789012345678123456781234"),
                new BigInteger("-12347890123456789012345678123456781234"),
                new BigInteger("0"),
        };
        output.writeObject(integers);
        output.flush();
    }

    public void customReplyTypedFixedList_CustomObject() throws Exception {
        Object[] objects = new Object[] {
                new BigInteger("1234"),
                new BigInteger("-12347890"),
                new BigInteger("0"),
                new BigDecimal("123.4"),
                new BigDecimal("-123.45"),
                new BigDecimal("0"),
        };
        output.writeObject(objects);
        output.flush();
    }

    public void customReplyTypedFixedIntegerZero() throws Exception {
        BigInteger integer = new BigInteger("0");
        output.writeObject(integer);
        output.flush();
    }

    public void customReplyTypedFixedIntegerSigned() throws Exception {
        BigInteger integer = new BigInteger("-4294967298");
        output.writeObject(integer);
        output.flush();
    }

    public void customReplyTypedFixedDecimal() throws Exception {
        BigDecimal decimal = new BigDecimal("100.256");
        output.writeObject(decimal);
        output.flush();
    }

    public void customReplyTypedFixedList_BigDecimal() throws Exception {
        BigDecimal[] decimals = new BigDecimal[] {
                new BigDecimal("123.4"),
                new BigDecimal("123.45"),
                new BigDecimal("123.456"),
        };
        output.writeObject(decimals);
        output.flush();
    }

    public void customReplyTypedFixedDateNull() throws Exception {
        DateDemo demo = new DateDemo("zhangshan", null, null);
        output.writeObject(demo);
        output.flush();
    }

    public void customReplyPerson183() throws Exception {
        Person183 p = new Person183();
        p.name = "pname";
        p.age = 13;
        InnerPerson innerPerson = new InnerPerson();
        innerPerson.name = "pname2";
        innerPerson.age = 132;
        p.innerPerson = innerPerson;
        output.writeObject(p);
        output.flush();
    }

    public void customReplyComplexString() throws Exception {
        output.writeObject(TestString.getComplexString());
        output.flush();
    }

    public void customReplySuperComplexString() throws Exception {
        output.writeObject(TestString.getSuperComplexString());
        output.flush();
    }

    public void customReplyStringEmoji() throws Exception {
        output.writeObject(TestString.getEmojiTestString());
        output.flush();
    }

    public void customReplyExtendClass() throws Exception {
        Dog dog = new Dog();
        dog.name = "a dog";
        dog.gender = "male";
        output.writeObject(dog);
        output.flush();
    }

    public void customReplyExtendClassToSingleStruct() throws Exception {
        Dog dog = new DogAll();
        dog.name = "a dog";
        dog.gender = "male";
        output.writeObject(dog);
        output.flush();
    }

    public void customReplyTypedFixedList_HashSet() throws Exception {
        Set<Integer> set = new HashSet<>();
        set.add(0);
        set.add(1);
        output.writeObject(set);
        output.flush();
    }

    public void customReplyTypedFixedList_HashSetCustomObject() throws Exception {
        Set<Object> set = new HashSet<>();
        set.add(new BigInteger("1234"));
        set.add(new BigDecimal("123.4"));
        output.writeObject(set);
        output.flush();
    }

    public void customReplyMap() throws Exception {
        Map<String, Object> map = new HashMap<String, Object>(4);
        map.put("a", 1);
        map.put("b", 2);
        output.writeObject(map);
        output.flush();
    }

    public Map<String, Object> mapInMap() throws Exception {
        Map<String, Object> map1 = new HashMap<String, Object>();
        map1.put("a", 1);
        Map<String, Object> map2 = new HashMap<String, Object>();
        map2.put("b", 2);

        Map<String, Object> map = new HashMap<String, Object>();
        map.put("obj1", map1);
        map.put("obj2", map2);
        return map;
    }

    public void customReplyMapInMap() throws Exception {
        output.writeObject(mapInMap());
        output.flush();
    }

    public void customReplyMapInMapJsonObject() throws Exception {
        JSONObject json = JSON.parseObject(JSON.toJSONString(mapInMap()));
        output.writeObject(json);
        output.flush();
    }

    public void customReplyGenericResponseLong() throws Exception {
        Response<Long> response = new Response<>(200, 123L);
        output.writeObject(response);
        output.flush();
    }

    public void customReplyGenericResponseBusinessData() throws Exception {
        Response<BusinessData> response = new Response<>(201, new BusinessData("apple", 5));
        output.writeObject(response);
        output.flush();
    }
}

interface Leg {
    public int legConnt = 4;
}

class Animal {
    public String name;
}

class Dog extends Animal implements Serializable, Leg {
    public String gender;
}

class DogAll extends Dog {
    public boolean all = true;
}

class TypedListTest implements Serializable {
    public A0 a;
    public A0[][] list;
    public A1[][] list1;

    TypedListTest() {
        this.a = new A0();
        this.list = new A0[][] { { new A0(), new A0() }, { new A0(), new A0() } };
        this.list1 = new A1[][] { { new A1(), new A1() }, { new A1(), new A1() } };
    }

}

class Person183 implements Serializable {
    public String name;
    public Integer age;
    public InnerPerson innerPerson;
}

class InnerPerson implements Serializable {
    public String name;
    public Integer age;
}
