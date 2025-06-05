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


import com.alibaba.dubbo.rpc.service.GenericException;

import java.io.*;
import java.lang.annotation.AnnotationTypeMismatchException;
import java.lang.annotation.IncompleteAnnotationException;
import java.lang.instrument.IllegalClassFormatException;
import java.lang.instrument.UnmodifiableClassException;
import java.lang.invoke.LambdaConversionException;
import java.lang.invoke.WrongMethodTypeException;
import java.lang.reflect.*;
import java.time.DateTimeException;
import java.time.format.DateTimeParseException;
import java.time.temporal.UnsupportedTemporalTypeException;
import java.time.zone.ZoneRulesException;
import java.util.*;
import java.util.concurrent.*;
import java.util.jar.JarException;
import java.util.prefs.BackingStoreException;
import java.util.prefs.InvalidPreferencesFormatException;
import java.util.zip.DataFormatException;
import java.util.zip.ZipException;

public class TestThrowable {
  public static Object throw_exception() {
    return new Exception("exception");
  }

  public static Object throw_throwable() {
    return new Throwable("exception");
  }

  public static Object throw_TypeNotPresentException() {
    return new TypeNotPresentException("exceptiontype1", new Throwable("exception"));
  }

  public static Object throw_UndeclaredThrowableException() {
    return new UndeclaredThrowableException(new Throwable(), "UndeclaredThrowableException");
  }

  public static Object throw_MalformedParametersException() {
    return new MalformedParametersException("MalformedParametersException");
  }

  public static Object throw_WrongMethodTypeException() {
    return new WrongMethodTypeException("WrongMethodTypeException");
  }

  public static Object throw_MalformedParameterizedTypeException() {
    return new MalformedParameterizedTypeException();
  }

  public static Object throw_uncheckedIOException() {
    return new java.io.UncheckedIOException(
            "uncheckedIOException", new java.io.IOException("io exception"));
  }

  public static Object throw_runtimeException() {
    return new RuntimeException("runtimeException");
  }

  public static Object throw_illegalStateException() {
    return new IllegalStateException("illegalStateException");
  }

  public static Object throw_illegalMonitorStateException() {
    return new IllegalMonitorStateException("illegalMonitorStateException");
  }

  public static Object throw_enumConstantNotPresentException() {
    return new EnumConstantNotPresentException(TestEnum.class, "enumConstantNotPresentException");
  }

  public static Object throw_classCastException() {
    return new ClassCastException("classCastException");
  }

  public static Object throw_arrayStoreException() {
    return new ArrayStoreException("arrayStoreException");
  }

  public static Object throw_IOException() {
    return new ArrayStoreException("IOException");
  }

  public static Object throw_NullPointerException() {
    return new NullPointerException("nullPointerException");
  }

  public static Object throw_UncheckedIOException() {
    return new UncheckedIOException("uncheckedIOException", new IOException("IOException"));
  }

  public static Object throw_FileNotFoundException() {
    return new FileNotFoundException("fileNotFoundException");
  }

  public static Object throw_EOFException() {
    return new EOFException("EOFException");
  }

  public static Object throw_SyncFailedException() {
    return new SyncFailedException("syncFailedException");
  }

  public static Object throw_ObjectStreamException() {
    return new InvalidObjectException("objectStreamException");
  }

  public static Object throw_WriteAbortedException() {
    return new WriteAbortedException("writeAbortedException", new Exception("detail"));
  }

  public static Object throw_InvalidObjectException() {
    return new InvalidObjectException("invalidObjectException");
  }

  public static Object throw_StreamCorruptedException() {
    return new StreamCorruptedException("streamCorruptedException");
  }

  public static Object throw_InvalidClassException() {
    return new InvalidClassException("invalidClassException");
  }

  public static Object throw_OptionalDataException()
          throws InvocationTargetException, NoSuchMethodException, IllegalAccessException,
          InstantiationException {
    Constructor c1 = OptionalDataException.class.getDeclaredConstructor(int.class);
    c1.setAccessible(true);
    return c1.newInstance(1);
  }

  public static Object throw_NotActiveException() {
    return new NotActiveException("notActiveException");
  }

  public static Object throw_NotSerializableException() {
    return new NotSerializableException("notSerializableException");
  }

  public static Object throw_UTFDataFormatException() {
    return new UTFDataFormatException("UTFDataFormatException");
  }

  public static Object throw_CloneNotSupportedException() {
    return new CloneNotSupportedException("CloneNotSupportedException");
  }

  public static Object throw_InterruptedException() {
    return new InterruptedException("InterruptedException");
  }

  public static Object throw_InterruptedIOException() {
    return new InterruptedIOException("InterruptedIOException");
  }

  public static Object throw_LambdaConversionException() {
    return new LambdaConversionException("LambdaConversionException");
  }

  public static Object throw_UnmodifiableClassException() {
    return new UnmodifiableClassException("UnmodifiableClassException");
  }

  public static Object throw_SecurityException() {
    return new SecurityException("SecurityException");
  }

  public static Object throw_IllegalArgumentException() {
    return new IllegalArgumentException("IllegalArgumentException");
  }

  public static Object throw_IllegalThreadStateException() {
    return new IllegalThreadStateException("IllegalThreadStateException");
  }

  public static Object throw_NumberFormatException() {
    return new NumberFormatException("NumberFormatException");
  }

  public static Object throw_IndexOutOfBoundsException() {
    return new IndexOutOfBoundsException("IndexOutOfBoundsException");
  }

  public static Object throw_ArrayIndexOutOfBoundsException() {
    return new ArrayIndexOutOfBoundsException("ArrayIndexOutOfBoundsException");
  }

  public static Object throw_StringIndexOutOfBoundsException() {
    return new StringIndexOutOfBoundsException("StringIndexOutOfBoundsException");
  }

  enum TestEnum {
    PASS
  }

  public static Object throw_IllegalFormatWidthException() {
    return new IllegalFormatWidthException(1000);
  }

  public static Object throw_IllegalFormatConversionException() {
    return new IllegalFormatConversionException('7', TestEnum.class);
  }

  public static Object throw_DuplicateFormatFlagsException() {
    return new DuplicateFormatFlagsException("DuplicateFormatFlagsException");
  }

  public static Object throw_MissingResourceException() {
    return new MissingResourceException(
            "MissingResourceException", "MissingResourceExceptionClass", "MissingResourceExceptionKey");
  }

  public static Object throw_ConcurrentModificationException() {
    return new ConcurrentModificationException("ConcurrentModificationException");
  }

  public static Object throw_RejectedExecutionException() {
    return new RejectedExecutionException("RejectedExecutionException");
  }

  public static Object throw_CompletionException() {
    return new CompletionException(new Throwable("exception"));
  }

  public static Object throw_EmptyStackException() {
    EmptyStackException e = new EmptyStackException();
    return e;
  }

  public static Object throw_IllformedLocaleException() {
    IllformedLocaleException e = new IllformedLocaleException("IllformedLocaleException");
    return e;
  }

  public static Object throw_NoSuchElementException() {
    return new NoSuchElementException("NoSuchElementException");
  }

  public static Object throw_NegativeArraySizeException() {
    return new NegativeArraySizeException("NegativeArraySizeException");
  }

  public static Object throw_UnsupportedOperationException() {
    return new UnsupportedOperationException("UnsupportedOperationException");
  }

  public static Object throw_ArithmeticException() {
    return new ArithmeticException("ArithmeticException");
  }

  public static Object throw_InputMismatchException() {
    return new InputMismatchException("InputMismatchException");
  }

  public static Object throw_ExecutionException() {
    return new ExecutionException("ExecutionException", new Throwable("exception"));
  }

  public static Object throw_InvalidPreferencesFormatException() {
    return new InvalidPreferencesFormatException("InvalidPreferencesFormatException", new Throwable("exception"));
  }

  public static Object throw_TimeoutException() {
    return new TimeoutException("TimeoutException");
  }

  public static Object throw_BackingStoreException() {
    return new BackingStoreException("BackingStoreException");
  }

  public static Object throw_DataFormatException() {
    return new DataFormatException("DataFormatException");
  }

  public static Object throw_BrokenBarrierException() {
    return new BrokenBarrierException("BrokenBarrierException");
  }

  public static Object throw_TooManyListenersException() {
    return new TooManyListenersException("TooManyListenersException");
  }

  public static Object throw_InvalidPropertiesFormatException() {
    return new InvalidPropertiesFormatException("InvalidPropertiesFormatException");
  }

  public static Object throw_ZipException() {
    return new ZipException("ZipException");
  }

  public static Object throw_JarException() {
    return new JarException("JarException");
  }

  public static Object throw_IllegalClassFormatException() {
    return new IllegalClassFormatException("IllegalClassFormatException");
  }

  public static Object throw_ReflectiveOperationException() {
    return new ReflectiveOperationException("ReflectiveOperationException", new Throwable("exception"));
  }

  public static Object throw_InvocationTargetException() {
    return new InvocationTargetException(new Throwable("exception"), "InvocationTargetException");
  }

  public static Object throw_NoSuchMethodException() {
    return new NoSuchMethodException("NoSuchMethodException");
  }

  public static Object throw_NoSuchFieldException() {
    return new NoSuchFieldException("NoSuchFieldException");
  }

  public static Object throw_IllegalAccessException() {
    return new IllegalAccessException("IllegalAccessException");
  }

  public static Object throw_ClassNotFoundException() {
    return new ClassNotFoundException("ClassNotFoundException", new Throwable("exception"));
  }

  public static Object throw_InstantiationException() {
    return new InstantiationException("InstantiationException");
  }

  public static Object throw_DateTimeException() {
    return new DateTimeException("DateTimeException", new Throwable("exception"));
  }

  public static Object throw_UnsupportedTemporalTypeException() {
    return new UnsupportedTemporalTypeException("UnsupportedTemporalTypeException", new Throwable("exception"));
  }

  public static Object throw_ZoneRulesException() {
    return new ZoneRulesException("ZoneRulesException", new Throwable("exception"));
  }

  public static Object throw_DateTimeParseException() {
    return new DateTimeParseException("DateTimeParseException", "CharSequence", 1, new Throwable("exception"));
  }

  public static Object throw_FormatterClosedException() {
    return new FormatterClosedException();
  }

  public static Object throw_CancellationException() {
    return new CancellationException("CancellationException");
  }

  public static Object throw_UnknownFormatConversionException() {
    return new UnknownFormatConversionException("UnknownFormatConversionException");
  }

  public static Object throw_UnknownFormatFlagsException() {
    return new UnknownFormatFlagsException("UnknownFormatFlagsException");
  }

  public static Object throw_IllegalFormatFlagsException() {
    return new IllegalFormatFlagsException("IllegalFormatFlagsException");
  }

  public static Object throw_IllegalFormatPrecisionException() {
    return new IllegalFormatPrecisionException(1);
  }

  public static Object throw_IllegalFormatCodePointException() {
    return new IllegalFormatCodePointException(1);
  }

  public static Object throw_MissingFormatArgumentException() {
    return new MissingFormatArgumentException("MissingFormatArgumentException");
  }

  public static Object throw_MissingFormatWidthException() {
    return new MissingFormatWidthException("MissingFormatWidthException");
  }

  public static Object throw_DubboGenericException() {
    return new GenericException("DubboGenericExceptionClass","DubboGenericException");
  }

  public static Object throw_IncompleteAnnotationException() {
    return new IncompleteAnnotationException(Override.class, "IncompleteAnnotationException");
  }

  public static Object throw_AnnotationTypeMismatchException() {
    return new AnnotationTypeMismatchException(Override.class.getEnclosingMethod(), "AnnotationTypeMismatchException");
  }

  public static Object throw_UserDefindException() {
      return new UserDefindException("throw UserDefindException");
  }

}
