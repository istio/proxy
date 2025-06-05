// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-change-array-by-copy

assertEquals(1, Array.prototype.toSorted.length);
assertEquals("toSorted", Array.prototype.toSorted.name);

function TerribleCopy(input) {
  let copy;
  if (Array.isArray(input)) {
    copy = [...input];
  } else {
    copy = { length: input.length };
    for (let i = 0; i < input.length; i++) {
      copy[i] = input[i];
    }
  }
  return copy;
}

function AssertToSortedAndSortSameResult(input, ...args) {
  const orig = TerribleCopy(input);
  const s = Array.prototype.toSorted.apply(input, args);
  const copy = TerribleCopy(input);
  Array.prototype.sort.apply(copy, args);

  // The in-place sorted version should be pairwise equal to the toSorted,
  // modulo being an actual Array if the input is generic.
  if (Array.isArray(input)) {
    assertEquals(copy, s);
  } else {
    assertEquals(copy.length, s.length);
    for (let i = 0; i < copy.length; i++) {
      assertEquals(copy[i], s[i]);
    }
  }

  // The original input should be unchanged.
  assertEquals(orig, input);

  // The result of toSorted() is a copy.
  assertFalse(s === input);
}

function TestToSortedBasicBehaviorHelper(input) {
  // No custom comparator.
  AssertToSortedAndSortSameResult(input);
  // Custom comparator.
  AssertToSortedAndSortSameResult(input, (x, y) => {
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
  });
}

// Smi packed
AssertToSortedAndSortSameResult([1,3,2,4]);

// Double packed
AssertToSortedAndSortSameResult([1.1,3.3,2.2,4.4]);

// Packed
AssertToSortedAndSortSameResult([true,false,1,42.42,null,"foo"]);

// Smi holey
AssertToSortedAndSortSameResult([1,,3,,2,,4,,]);

// Double holey
AssertToSortedAndSortSameResult([1.1,,3.3,,2.2,,4.4,,]);

// Holey
AssertToSortedAndSortSameResult([true,,false,,1,,42.42,,null,,"foo",,]);

// Generic
AssertToSortedAndSortSameResult({ length: 4,
                                  get "0"() { return "hello"; },
                                  get "1"() { return "cursed"; },
                                  get "2"() { return "java"; },
                                  get "3"() { return "script" } });

(function TestSnapshotAtBeginning() {
  const a = [1,3,4,2];
  // Use a cursed comparator that mutates the original array. toSorted, like
  // sort, takes a snapshot at the beginning.
  const s = a.toSorted((x, y) => {
    a.pop();
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
  });
  assertEquals([1,2,3,4], s);
  assertEquals(0, a.length);
})();

(function TestTooBig() {
  const a = { length: Math.pow(2, 32) };
  assertThrows(() => Array.prototype.toSorted.call(a), RangeError);
})();

(function TestNoSpecies() {
  class MyArray extends Array {
    static get [Symbol.species]() { return MyArray; }
  }
  assertEquals(Array, (new MyArray()).toSorted().constructor);
})();

// All tests after this have an invalidated elements-on-prototype protector.
(function TestNoHoles() {
  const a = [,,,,];
  Array.prototype[3] = "on proto";
  const s = a.toSorted();
  assertEquals(["on proto",undefined,undefined,undefined], s);
  assertEquals(a.length, s.length)
  for (let i = 0; i < a.length; i++) {
    assertFalse(a.hasOwnProperty(i));
    assertTrue(s.hasOwnProperty(i));
  }
})();

assertEquals(Array.prototype[Symbol.unscopables].toSorted, true);
