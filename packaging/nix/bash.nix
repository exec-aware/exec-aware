# SPDX-FileCopyrightText: 2026 Skye Soss <skye@soss.website>
#
# SPDX-License-Identifier: Apache-2.0

{
  bash,
  fetchgit,
}:
bash.overrideAttrs {
  version = "5.3.20";

  # Bash 5.3 with upstream patches 001-020 applied
  src = fetchgit {
    url = "https://git.savannah.gnu.org/git/bash.git";
    rev = "9c465866b7d849378369ef700cbe2965aa9691e3";
    hash = "sha256-epE1HoQcAAweucfA4achPSGrfeVlSNqWo5LptisiL54=";
  };

  # Replaces the nixpkgs patches: the upstream patches are already in src,
  # and pgrp-pipe-5.patch only matters when building on a non-Linux host.
  patchFlags = [ "-p1" ];
  patches = [
    ../../patches/bash/v5.3/01-exec-aware-bash.patch
  ];
}
