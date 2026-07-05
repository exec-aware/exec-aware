# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  busybox,
}:

stdenv.mkDerivation {
  name = "test-busybox-exec-aware";
  src = ../../tests/busybox;

  makeFlags = [ "BUSYBOX=${busybox}/bin/busybox" ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
