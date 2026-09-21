# Test WebAssembly modules

## attributegen.wasm

Prebuilt WebAssembly module used only by the end-to-end test
`TestAttributeGen` in `test/envoye2e/stats_plugin/stats_test.go`.

The attributegen source was removed from this repository in
https://github.com/istio/proxy/pull/4462, so this artifact is frozen. It was
previously downloaded at test time from
`https://storage.googleapis.com/istio-build/proxy/attributegen-359dcd3a19f109c50e97517fe6b1e2676e870c4d.wasm`
(built from istio/proxy commit `359dcd3a19f109c50e97517fe6b1e2676e870c4d`), and
is now checked in so that tests work offline.

SHA256: `3c807c3f48af481e4dda7d9fe54364d921bb7e88122765f67f6c9b9cb28a52a6`
