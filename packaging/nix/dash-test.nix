# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  dash,
}:

stdenv.mkDerivation {
  name = "test-dash-exec-aware";
  src = ../../tests/dash;

  makeFlags = [ "DASH=${dash}/bin/dash" ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
