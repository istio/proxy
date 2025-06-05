// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test inspection of Wasm anyref objects');
session.setupScriptMap();
Protocol.Runtime.enable();
Protocol.Debugger.enable();
Protocol.Debugger.onPaused(printPauseLocationsAndContinue);

let breakpointLocation = -1;

InspectorTest.runAsyncTestSuite([
  async function test() {
    instantiateWasm();
    let scriptIds = await waitForWasmScripts();

    // Set a breakpoint.
    InspectorTest.log('Setting breakpoint');
    let breakpoint = await Protocol.Debugger.setBreakpoint(
        {'location': {'scriptId': scriptIds[0],
                      'lineNumber': 0,
                      'columnNumber': breakpointLocation}});
    printIfFailure(breakpoint);
    InspectorTest.logMessage(breakpoint.result.actualLocation);

    // Now run the wasm code.
    await WasmInspectorTest.evalWithUrl('instance.exports.main()', 'runWasm');
    InspectorTest.log('exports.main returned. Test finished.');
  }
]);

async function printPauseLocationsAndContinue(msg) {
  let loc = msg.params.callFrames[0].location;
  InspectorTest.log('Paused:');
  await session.logSourceLocation(loc);
  InspectorTest.log('Scope:');
  for (var frame of msg.params.callFrames) {
    var isWasmFrame = /^wasm/.test(frame.url);
    var functionName = frame.functionName || '(anonymous)';
    var lineNumber = frame.location.lineNumber;
    var columnNumber = frame.location.columnNumber;
    InspectorTest.log(`at ${functionName} (${lineNumber}:${columnNumber}):`);
    for (var scope of frame.scopeChain) {
      InspectorTest.logObject(' - scope (' + scope.type + '):');
      if (!isWasmFrame && scope.type == 'global') {
        // Skip global scope for non wasm-functions.
        InspectorTest.logObject('   -- skipped globals');
        continue;
      }
      var properties = await Protocol.Runtime.getProperties(
          {'objectId': scope.object.objectId});
      await WasmInspectorTest.dumpScopeProperties(properties);
    }
  }
  InspectorTest.log();
  Protocol.Debugger.resume();
}

async function instantiateWasm() {
  var builder = new WasmModuleBuilder();
  let struct_type = builder.addStruct([makeField(kWasmI32, false)]);
  let array_type = builder.addArray(kWasmI32);
  let ref_table = builder.addTable(kWasmAnyRef, 4)
                         .exportAs('exported_ref_table');

  builder.addFunction('fill_ref_table', kSig_v_v)
    .addBody([
      ...wasmI32Const(0), ...wasmI32Const(123),
      kGCPrefix, kExprStructNew, struct_type, kExprTableSet, ref_table.index,
      ...wasmI32Const(1), ...wasmI32Const(20), ...wasmI32Const(21),
      kGCPrefix, kExprArrayNewFixed, array_type, 2,
      kExprTableSet, ref_table.index,
      // TODO(7748): Reactivate this test when JS interop between i31refs and
      // JS SMIs is fixed. The problem right now is the 33-bit shift for i31ref
      // values on non-pointer-compressed platforms, which means i31refs and
      // Smis have different encodings there but it's impossible to tell them
      // apart.
      // ...wasmI32Const(2), ...wasmI32Const(30),
      // kGCPrefix, kExprI31New, kExprTableSet, ref_table.index,
    ]).exportFunc();

  let body = [
    // Set local anyref_local to new struct.
    ...wasmI32Const(12),
    kGCPrefix, kExprStructNew, struct_type,
    kExprLocalSet, 0,
    // Set local anyref_local2 to new array.
    ...wasmI32Const(21),
    kGCPrefix, kExprArrayNewFixed, array_type, 1,
    kExprLocalSet, 1,
    // Set local anyref_local_i31.
    // TODO(7748): Reactivate this test when JS interop between i31refs and JS
    // SMIs is fixed (same issue as above).
    // ...wasmI32Const(30),
    // kGCPrefix, kExprI31New,
    // kExprLocalSet, 2,
    kExprNop,
  ];
  let main = builder.addFunction('main', kSig_v_v)
      .addLocals(kWasmAnyRef, 1, ['anyref_local'])
      .addLocals(kWasmAnyRef, 1, ['anyref_local2'])
      .addLocals(kWasmAnyRef, 1, ['anyref_local_i31'])
      .addLocals(kWasmAnyRef, 1, ['anyref_local_null'])
      .addBody(body)
      .exportFunc();

  var module_bytes = builder.toArray();
  breakpointLocation = main.body_offset + body.length - 1;

  InspectorTest.log('Calling instantiate function.');
  await WasmInspectorTest.instantiate(module_bytes);
  InspectorTest.log('Module instantiated.');
  await WasmInspectorTest.evalWithUrl(
      'instance.exports.fill_ref_table()', 'fill_ref_table');
  InspectorTest.log('Table populated.');
}

async function waitForWasmScripts() {
  InspectorTest.log('Waiting for wasm script to be parsed.');
  let wasm_script_ids = [];
  while (wasm_script_ids.length < 1) {
    let script_msg = await Protocol.Debugger.onceScriptParsed();
    let url = script_msg.params.url;
    if (url.startsWith('wasm://')) {
      InspectorTest.log('Got wasm script!');
      wasm_script_ids.push(script_msg.params.scriptId);
    }
  }
  return wasm_script_ids;
}
