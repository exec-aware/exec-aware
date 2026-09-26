# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  glibc,
}:

stdenv.mkDerivation {
  name = "test-glibc-exec-aware";
  src = ../../tests/glibc;

  # The test programs must only find the glibc under test
  env.NIX_DONT_SET_RPATH = true;

  makeFlags = [
    "LDSO=${glibc}/lib/${baseNameOf stdenv.cc.bintools.dynamicLinker}"
  ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
