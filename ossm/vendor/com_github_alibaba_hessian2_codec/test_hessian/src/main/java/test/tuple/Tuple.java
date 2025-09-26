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

package test.tuple;

import java.io.Serializable;

public class Tuple implements Serializable {
    private static final long serialVersionUID = -1L;

    Integer Integer;
    Byte Byte;
    Short Short;
    Long Long;
    Double Double;
    int i;
    byte b;
    short s;
    long l;
    double d;


    public java.lang.Integer getInteger() {
        return Integer;
    }

    public void setInteger(java.lang.Integer integer) {
        Integer = integer;
    }

    public java.lang.Byte getByte() {
        return Byte;
    }

    public void setByte(java.lang.Byte aByte) {
        Byte = aByte;
    }

    public java.lang.Short getShort() {
        return Short;
    }

    public void setShort(java.lang.Short aShort) {
        Short = aShort;
    }

    public java.lang.Long getLong() {
        return Long;
    }

    public void setLong(java.lang.Long aLong) {
        Long = aLong;
    }

    public int getI() {
        return i;
    }

    public void setI(int i) {
        this.i = i;
    }

    public byte getB() {
        return b;
    }

    public void setB(byte b) {
        this.b = b;
    }

    public short getS() {
        return s;
    }

    public void setS(short s) {
        this.s = s;
    }

    public long getL() {
        return l;
    }

    public void setL(long l) {
        this.l = l;
    }

    public java.lang.Double getDouble() {
        return Double;
    }

    public void setDouble(java.lang.Double aDouble) {
        Double = aDouble;
    }

    public double getD() {
        return d;
    }

    public void setD(double d) {
        this.d = d;
    }
}