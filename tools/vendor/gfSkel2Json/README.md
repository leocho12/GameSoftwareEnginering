# gfSkel2Json provenance

Offline asset converter, not linked into SimpleGame.

- Source: https://github.com/baguniz/gfSkel2Json
- File: SkeletonBinary.js, retrieved 2026-09-14 from master.
- Original Git blob SHA: 1d086e33c97a37b8506941765c9542ad6bb2a004.
- License: GPL-2.0, preserved in LICENSE.
- Local modifications: retain unweighted mesh FFD offsets at original scale; derive weighted FFD array length from actual vertex influence counts instead of a fixed multiplier.

The C++ game uses its own pre-baked GFA1 reader. Neither this converter nor a Spine runtime is needed during game execution.
