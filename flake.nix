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
