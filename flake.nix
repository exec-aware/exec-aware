# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  description = "Patches for exec-aware programs";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      inherit (nixpkgs) lib;
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forEachSystem = lib.genAttrs systems;
    in
    {

      devShells = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ ];
            packages = [
              pkgs.stgit
              pkgs.treefmt
              pkgs.nixfmt
              pkgs.reuse
            ];
          };
        }
      );

      packages = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          musl = pkgs.callPackage ./packaging/nix/musl.nix { };
          lua = pkgs.callPackage ./packaging/nix/lua.nix { inherit (pkgs) lua5_5; };
          busybox = pkgs.callPackage ./packaging/nix/busybox.nix { };
          util-linux = (pkgs.util-linux.override { translateManpages = false; }).overrideAttrs (
            finalAttrs: prevAttrs: {
              src = pkgs.fetchFromGitHub {
                owner = "Skyb0rg007";
                repo = "util-linux";
                rev = "87397f60013a9bfb4054ae1d673b8f9a0cef6062";
                hash = "sha256-0jc6M0uV9Zq3IrN1Cf/99ksWX/FJrZvNbK0DAkujcVA=";
              };
              patches = [ ];
              preConfigure = "./autogen.sh";
              nativeBuildInputs = prevAttrs.nativeBuildInputs ++ [
                pkgs.libtool
                pkgs.automake
                pkgs.flex
                pkgs.bison
                pkgs.gettext
                pkgs.asciidoctor
              ];
            }
          );
        }
      );

      checks = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          musl-exec-aware = pkgs.callPackage ./packaging/nix/musl-test.nix {
            musl = self.packages.${system}.musl;
          };
          lua-exec-aware = pkgs.callPackage ./packaging/nix/lua-test.nix {
            lua = self.packages.${system}.lua;
          };
          busybox-exec-aware = pkgs.callPackage ./packaging/nix/busybox-test.nix {
            busybox = self.packages.${system}.busybox;
          };
        }
      );

      formatter = forEachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        pkgs.writeShellApplication {
          name = "treefmt-wrapper";
          runtimeInputs = [
            pkgs.treefmt
            pkgs.nixfmt
          ];
          text = ''exec treefmt "$@"'';
        }
      );

    };
}
