# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  stdenv,
  python,
}:

stdenv.mkDerivation {
  name = "test-python-exec-aware";
  src = ../../tests/python;

  makeFlags = [ "PYTHON=${python.interpreter}" ];

  dontConfigure = true;
  doCheck = true;
  installPhase = "touch $out";
}
