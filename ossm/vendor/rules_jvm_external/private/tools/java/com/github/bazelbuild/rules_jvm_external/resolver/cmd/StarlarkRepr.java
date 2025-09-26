// Copyright 2015 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package com.github.bazelbuild.rules_jvm_external.resolver.cmd;

import java.util.Arrays;
import java.util.Collection;
import java.util.Map;

// Most of the code in here is gleefully ripped from Bazel's
// //bazel/src/main/java/net/starlark/java/eval:Printer.java
class StarlarkRepr {

  public String repr(Object o) {
    return new Printer().repr(o).toString();
  }

  private static class Printer {

    private final StringBuilder buffer = new StringBuilder();
    // Stack of values in the middle of being printed.
    // Each renders as "..." if recursively encountered,
    // indicating a cycle.
    private Object[] stack;
    private int depth;

    public Printer repr(Object o) {
      // atomic values (leaves of the object graph)
      if (o == null) {
        // Java null is not a valid Starlark value, but sometimes printers are used on non-Starlark
        // values such as Locations or Nodes.
        return this.append("None");
      } else if (o instanceof String) {
        appendQuoted((String) o);
        return this;
      } else if (o instanceof Boolean) {
        this.append(((boolean) o) ? "True" : "False");
        return this;
      } else if (o instanceof Integer) { // a non-Starlark value
        this.buffer.append((int) o);
        return this;
      }

      // compound values (may form cycles in the object graph)

      if (!push(o)) {
        return this.append("..."); // elided cycle
      }
      try {
        if (o instanceof Map) {
          Map<?, ?> dict = (Map<?, ?>) o;
          this.printList(dict.entrySet(), "{", ", ", "}");
        } else if (o instanceof Collection) {
          this.printList((Collection<?>) o, "[", ", ", "]");
        } else if (o instanceof Map.Entry) {
          Map.Entry<?, ?> entry = (Map.Entry<?, ?>) o;
          this.repr(entry.getKey());
          this.append(": ");
          this.repr(entry.getValue());
        } else {
          // All other non-Starlark Java values (e.g. Node, Location).
          // Starlark code cannot access values of o that would reach here,
          // and native code is already trusted to be deterministic.
          this.append(o.toString());
        }
      } finally {
        pop();
      }

      return this;
    }

    public String toString() {
      return buffer.toString();
    }

    private Printer appendQuoted(String s) {
      this.append('"');
      int len = s.length();
      for (int i = 0; i < len; i++) {
        char c = s.charAt(i);
        escapeCharacter(c);
      }
      return this.append('"');
    }

    private Printer escapeCharacter(char c) {
      if (c == '"') {
        return backslashChar(c);
      }
      switch (c) {
        case '\\':
          return backslashChar('\\');
        case '\r':
          return backslashChar('r');
        case '\n':
          return backslashChar('n');
        case '\t':
          return backslashChar('t');
        default:
          if (c < 32) {
            // TODO(bazel-team): support \x escapes
            return this.append(String.format("\\x%02x", (int) c));
          }
          return this.append(c); // no need to support UTF-8
      }
    }

    private Printer backslashChar(char c) {
      return this.append('\\').append(c);
    }

    private Printer printList(Iterable<?> list, String before, String separator, String after) {
      this.append(before);
      String sep = "";
      for (Object elem : list) {
        this.append(sep);
        sep = separator;
        this.repr(elem);
      }
      return this.append(after);
    }

    public final Printer append(char c) {
      buffer.append(c);
      return this;
    }

    public final Printer append(CharSequence s) {
      buffer.append(s);
      return this;
    }

    private boolean push(Object x) {
      // cyclic?
      for (int i = 0; i < depth; i++) {
        if (x == stack[i]) {
          return false;
        }
      }

      if (stack == null) {
        this.stack = new Object[4];
      } else if (depth == stack.length) {
        this.stack = Arrays.copyOf(stack, 2 * stack.length);
      }
      this.stack[depth++] = x;
      return true;
    }

    private void pop() {
      this.stack[--depth] = null;
    }
  }
}
