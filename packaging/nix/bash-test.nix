# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  bash,
}:

stdenv.mkDerivation {
  name = "test-bash-exec-aware";
  src = ../../tests/bash;

  makeFlags = [ "BASH=${bash}/bin/bash" ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
