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
          util-linux = (pkgs.util-linux.override { translateManpages = false; }).overrideAttrs (
            finalAttrs: prevAttrs: {
              src = pkgs.fetchFromGitHub {
                owner = "Skyb0rg007";
                repo = "util-linux";
                rev = "6d407908452cb50522d3a8ae8974f24499b43237";
                hash = "sha256-kvLDkFcF37NnOEoVbx9XcAcRfRC4+I6MHcGBI2T1KYQ=";
              };
              patches = [ ];
              preConfigure = "./autogen.sh";
              nativeBuildInputs = prevAttrs.nativeBuildInputs ++ [
                pkgs.autoreconfHook
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
