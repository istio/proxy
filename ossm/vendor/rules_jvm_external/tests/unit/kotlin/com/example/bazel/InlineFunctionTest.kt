package com.example.bazel

import org.junit.Test
import java.lang.UnsupportedOperationException
import kotlin.test.assertFailsWith
import kotlin.test.todo

class InlineFunctionTest {

  // If the kotlin.test JAR was imported with regular ijar-enabled java_import, we will see
  // the following message:
  //
  // exception: java.lang.IllegalStateException: Backend Internal error: Exception during code generation
  // Cause: Back-end (JVM) Internal error: Couldn't inline method call 'assertFailsWith' into
  // @org.junit.Test public final fun `test that inline functions like assertFailsWith isn't removed by ijar during import`(): kotlin.Unit defined in com.example.bazel.InlineFunctionTest
  // @Test fun `test that inline functions like assertFailsWith isn't removed by ijar during import`() {
  //   assertFailsWith<UnsupportedOperationException>("didn't fail") {
  //     throw UnsupportedOperationException()
  //   }
  // }
  // Cause: Not generated
  // Cause: Couldn't obtain compiled function body for public inline fun <reified T : kotlin.Throwable> assertFailsWith(message: kotlin.String? = ..., noinline block: () -> kotlin.Unit): T defined in kotlin.test[DeserializedSimpleFunctionDescriptor@105db94d]
  @Test fun `test that inline functions like assertFailsWith isn't removed by ijar during import`() {
    assertFailsWith<UnsupportedOperationException>("didn't fail") {
      throw UnsupportedOperationException()
    }
  }

  // For good measure, test another inlined function, `todo`.
  // https://kotlinlang.org/api/latest/kotlin.test/kotlin.test/todo.html
  @Test fun `test that the todo inlined function isn't removed by ijar during import`() {
    todo({ })
  }

}
