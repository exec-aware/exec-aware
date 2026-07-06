# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  lib,
  fetchurl,
  stdenv,
  autoreconfHook,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "dash";
  version = "0.5.13.4";

  src = fetchurl {
    url = "https://git.kernel.org/pub/scm/utils/dash/dash.git/snapshot/dash-${finalAttrs.version}.tar.gz";
    hash = "sha256-ZS6VAkt1dY3NFBtkoNOXOgJsxqruGuyBzgPnbcPmomc=";
  };

  patches = [
    ../../patches/dash/v0.5.13.4/01-exec-aware-dash.patch
  ];

  nativeBuildInputs = [ autoreconfHook ];

  installPhase = ''
    install -D -m 0755 src/dash $out/bin/dash
  '';

  enableParallelBuilding = true;

  meta = {
    homepage = "http://gondor.apana.org.au/~herbert/dash/";
    license = lib.licenses.bsd3;
  };
})
