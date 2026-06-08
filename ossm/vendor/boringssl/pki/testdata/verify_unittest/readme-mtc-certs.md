# MTC test cert generation

The following test certs are created according to these instructions.

## Certs

- `mtc-leaf.pem`
- `mtc-leaf-bitflip.pem`
  - a copy of `mtc-leaf.pem`, but with a bitflip in its inclusion proof
- `mtc-leaf-b.pem`
- `mtc-leaf-c.pem`

## Instructions

- Run
  `go run github.com/davidben/merkle-tree-certs/demo@92282dba2bf361c486dda5fe7606ef77abd2a1a0 -config=mtc-config.json`
- copy/move the following output files:
  - `out/cert_9_0.pem` to `mtc-leaf.pem`
  - `out/cert_9_1.pem` to `mtc-leaf-bitflip.pem`
  - `out/cert_10_0.pem` to `mtc-leaf-b.pem`
  - `out/cert_19_0.pem` to `mtc-leaf-c.pem`
- edit `VerifyMTCTest::SetUp` to set the trusted subtrees to the ones output by
  the above command.
- remove other artifacts created by the merkle-tree-certs/demo tool (e.g.
  `rm -r out`).