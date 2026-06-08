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
import com.caucho.hessian.test.A0;
import com.caucho.hessian.test.A1;
import java.io.InputStream;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.*;

import test.model.DateDemo;


public class TestCustomDecode {

    private Hessian2Input input;

    TestCustomDecode(InputStream is) {
        input = new Hessian2Input(is);
    }

    public Object customArgUntypedFixedListHasNull() throws Exception {
        List list = new ArrayList();
        list.add(new A0());
        list.add(new A1());
        list.add(null);

        Object o = input.readObject();
        return list.equals(o);
    }

    public Object customArgTypedFixedList() throws Exception {
        A0[] list = new A0[]{new A0()};
        Object o = input.readObject();
        return Arrays.equals(list, (A0[]) o);
    }

    public Object customArgTypedFixedList_short_0() throws Exception {
        short[] list = new short[]{};
        Object o = input.readObject();
        return Arrays.equals(list, (short[]) o);
    }

    public Object customArgTypedFixedList_short_7() throws Exception {
        short[] list = new short[]{1, 2, 3, 4, 5, 6, 7};
        Object o = input.readObject();
        return Arrays.equals(list, (short[]) o);
    }

    public Object customArgTypedFixedList_int_0() throws Exception {
        int[] list = new int[]{};
        Object o = input.readObject();
        return Arrays.equals(list, (int[]) o);
    }

    public Object customArgTypedFixedList_int_7() throws Exception {
        int[] list = new int[]{1, 2, 3, 4, 5, 6, 7};
        Object o = input.readObject();
        return Arrays.equals(list, (int[]) o);
    }

    public Object customArgTypedFixedList_long_0() throws Exception {
        long[] list = new long[]{};
        Object o = input.readObject();
        return Arrays.equals(list, (long[]) o);
    }

    public Object customArgTypedFixedList_long_7() throws Exception {
        long[] list = new long[]{1, 2, 3, 4, 5, 6, 7};
        Object o = input.readObject();
        return Arrays.equals(list, (long[]) o);
    }

    public Object customArgTypedFixedList_float_0() throws Exception {
        float[] list = new float[]{};
        Object o = input.readObject();
        return Arrays.equals(list, (float[]) o);
    }

    public Object customArgTypedFixedList_float_7() throws Exception {
        float[] list = new float[]{1, 2, 3, 4, 5, 6, 7};
        Object o = input.readObject();
        return Arrays.equals(list, (float[]) o);
    }

    public Object customArgTypedFixedList_double_0() throws Exception {
        double[] list = new double[]{};
        Object o = input.readObject();
        return Arrays.equals(list, (double[]) o);
    }

    public Object customArgTypedFixedList_double_7() throws Exception {
        double[] list = new double[]{1, 2, 3, 4, 5, 6, 7};
        Object o = input.readObject();
        return Arrays.equals(list, (double[]) o);
    }

    public Object customArgTypedFixedList_boolean_0() throws Exception {
        boolean[] list = new boolean[]{};
        Object o = input.readObject();
        return Arrays.equals(list, (boolean[]) o);
    }

    public Object customArgTypedFixedList_boolean_7() throws Exception {
        boolean[] list = new boolean[]{true, false, true, false, true, false, true};
        Object o = input.readObject();
        return Arrays.equals(list, (boolean[]) o);
    }

    public Object customArgTypedFixedList_date_0() throws Exception {
        Date[] list = new Date[]{};
        Object o = input.readObject();
        return Arrays.equals(list, (Date[]) o);
    }

    public Object customArgTypedFixedList_date_3() throws Exception {
        Date[] list = new Date[]{new Date(1560864000), new Date(1560864000), new Date(1560864000)};
        Object o = input.readObject();
        return Arrays.equals(list, (Date[]) o);
    }

    public Object customArgTypedFixedList_arrays() throws Exception {
        int[][][] list = new int[][][]{{{1, 2, 3}, {4, 5, 6, 7}}, {{8, 9, 10}, {11, 12, 13, 14}}};
        try {
            Object o = input.readObject();
            return Arrays.deepEquals(list, (int[][][]) o);
        } catch (Exception e) {
            return e.toString();
        }
    }

    public Object customArgTypedFixedList_A0arrays() throws Exception {
        A0[][][] list = new A0[][][]{{{new A0(), new A0(), new A0()}, {new A0(), new A0(), new A0(), null}}, {{new A0()}, {new A0()}}};
        Object o = input.readObject();
        return Arrays.deepEquals(list, (A0[][][]) o);
    }

    public Object customArgTypedFixedList_Test() throws Exception {
        TypedListTest t = new TypedListTest();
        Object o = input.readObject();
        TypedListTest t2 = (TypedListTest) o;
        return t.a.equals(t.a) && Arrays.deepEquals(t.list, t2.list) && Arrays.deepEquals(t.list1, t2.list1);
    }

    public Object customArgTypedFixedList_Object() throws Exception {
        Object[] list = new Object[]{new A0()};
        Object o = input.readObject();
        return Arrays.deepEquals(list, (Object[]) o);
    }

    public Object customArgTypedFixed_Integer() throws Exception {
        BigInteger o = (BigInteger) input.readObject();
        return o.toString().equals("4294967298");
    }

    public Object customArgTypedFixed_IntegerZero() throws Exception {
        BigInteger o = (BigInteger) input.readObject();
        return o.toString().equals("0");
    }

    public Object customArgTypedFixed_IntegerSigned() throws Exception {
        BigInteger o = (BigInteger) input.readObject();
        return o.toString().equals("-4294967298");
    }

    public Object customArgTypedFixed_Decimal() throws Exception {
        BigDecimal o = (BigDecimal) input.readObject();
        return o.toString().equals("100.256");
    }

    public Object customArgTypedFixed_Extends() throws Exception {
        Dog o = (Dog) input.readObject();
        return o.name.equals("a dog") && o.gender.equals("male");
    }

    public Object customArgTypedFixed_DateNull() throws Exception {
        DateDemo o = (DateDemo) input.readObject();
        return o.getDate() == null && o.getDate1() == null;
    }

    public Object customArgString_emoji() throws Exception {
        String o = (String) input.readObject();
        return TestString.getEmojiTestString().equals(o);
    }

    public Object customArgComplexString() throws Exception {
        String o = (String) input.readObject();
        return TestString.getComplexString().equals(o);
    }

    public Object customArgTypedFixedList_HashSet() throws Exception {
        HashSet o = (HashSet) input.readObject();
        return o.contains(0) && o.contains(1);
    }
}