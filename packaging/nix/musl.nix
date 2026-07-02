{
  lib,
  fetchurl,
  stdenv,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "musl";
  version = "1.2.6";

  src = fetchurl {
    url = "https://musl.libc.org/releases/musl-${finalAttrs.version}.tar.gz";
    hash = "sha256-1YX9O2E8ZhUfwySejtRPdwIMtebB5jWmFtP5+CRgUSo=";
  };

  patches = [
    (fetchurl {
      name = "CVE-2026-6042.patch";
      url = "https://www.openwall.com/lists/musl/2026/04/03/2/1";
      hash = "sha256-RE+nDlLKFY+31LrVYGN3kLv49y6AuC//hA3Wb6gwkeM=";
    })
    (fetchurl {
      name = "CVE-2026-40200.patch";
      url = "https://www.openwall.com/lists/musl/2026/04/10/3/1";
      hash = "sha256-HuKfZPnKjorXw0l3nWYf9rUhJqJ1ddNYaYE1elLEBvs=";
    })
    ../../patches/musl/v1.2.6/01-exec-aware-dlopen.patch
  ];

  env.NIX_DONT_SET_RPATH = true;

  configureFlags = [
    "--enable-shared"
    "--enable-static"
    "--enable-debug"
    "--enable-wrapper=all"
    "--syslibdir=${placeholder "out"}/lib"
  ];

  enableParallelBuilding = true;
  dontDisableStatus = true;
  dontAddStaticConfigureFlags = true;

  meta = {
    homepage = "https://musl.libc.org/";
    license = lib.licenses.mit;
  };
})
