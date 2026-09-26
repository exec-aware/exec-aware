# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  perl,
}:

stdenv.mkDerivation {
  name = "test-perl-exec-aware";
  src = ../../tests/perl;

  makeFlags = [ "PERL=${perl}/bin/perl" ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
