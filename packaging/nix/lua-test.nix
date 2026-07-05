# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  lua,
}:

stdenv.mkDerivation {
  name = "test-lua-exec-aware";
  src = ../../tests/lua;

  nativeBuildInputs = [ lua ];

  makeFlags = [ "LUA=${lua}/bin/lua" ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
