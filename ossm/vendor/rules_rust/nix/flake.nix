{
  inputs = {
    # Avoid need for https://github.com/nix-community/fenix/pull/180
    nixpkgs = {
      url = "github:NixOS/nixpkgs?rev=614462224f836ca340aed96b86799ad09b4c2298";
    };

    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = pkgs.mkShell
          {
            packages = [
              pkgs.bazel-buildtools
              pkgs.bazel_7
              pkgs.cargo
              pkgs.rustc
              pkgs.rustfmt
            ];
          };
      });
}
