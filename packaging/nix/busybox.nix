# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  lib,
  fetchurl,
  stdenv,
  perl,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "busybox";
  version = "1.38.0";

  src = fetchurl {
    url = "https://www.busybox.net/downloads/busybox-${finalAttrs.version}.tar.bz2";
    hash = "sha256-NPnqb/hjbyySQRU7kRTu+p5lZ0pFMYrh75W7XzHFO7I=";
  };

  patches = [
    ../../patches/busybox/v1.38.0/01-exec-aware-ash.patch
    ../../patches/busybox/v1.38.0/02-exec-aware-hush.patch
    ../../patches/busybox/v1.38.0/03-exec-aware-awk.patch
  ];

  nativeBuildInputs = [ perl ];

  configurePhase = ''
    make defconfig
    # CBQ queueing structures were removed from kernel headers in Linux 6.x;
    # disable the tc applet which depends on them.
    sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config
  '';

  installPhase = ''
    install -D -m 0755 busybox $out/bin/busybox
  '';

  enableParallelBuilding = true;

  meta = {
    homepage = "https://www.busybox.net/";
    license = lib.licenses.gpl2Only;
  };
})
