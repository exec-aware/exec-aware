# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  util-linux,
}:

stdenv.mkDerivation {
  name = "test-util-linux-exec-aware";
  src = ../../tests/util-linux;

  makeFlags = [ "UTIL_LINUX_BIN=${util-linux.bin}/bin" ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
