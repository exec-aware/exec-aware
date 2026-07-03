# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  musl,
  gcc,
  binutils,
}:

stdenv.mkDerivation {
  name = "test-musl-exec-aware";
  src = ../../tests/musl;

  nativeBuildInputs = [
    musl
    gcc.cc
    binutils
  ];

  REALGCC = "${gcc.cc}/bin/gcc";
  makeFlags = [ "CC=${musl}/bin/musl-gcc" ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
