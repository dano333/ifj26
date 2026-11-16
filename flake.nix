{
  description = "ifj26 project";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        llvmPkgs = pkgs.llvmPackages_23;
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = [
            # Build tools
            pkgs.gnumake

            # Compiler & LLVM tooling
            pkgs.gcc
            llvmPkgs.clang-tools

            # Debugging
            pkgs.gdb

            # LaTeX tooling
            pkgs.texliveMedium
          ];

          shellHook = ''
            export CC=clang
            export CXX=clang++
          '';
        };
      }
    );
}