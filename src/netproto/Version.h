// TokelaCoop release version - the ONE place it is written.
//
// Shown in game (the top-left banner, the F2 panel title), written to the log at
// startup, stamped into the kit (scripts/make_mod_kit.ps1 reads this line) and
// into the .mod description. Change it ONLY with scripts/set_version.ps1, which
// also updates the tracked .mod description, the generator and the README, then
// tag: the release job in .github/workflows/build.yml refuses a tag that is not
// "v" + this value.
//
// It is NOT the wire protocol version (PROTOCOL_VERSION in Wire.h), which only
// changes when a packet does, and it never goes over the wire. Up to v0.53 the
// project was called KenshiCoop.
#ifndef TOKELACOOP_VERSION_H
#define TOKELACOOP_VERSION_H

#define TOKELACOOP_VERSION "0.54"
#define TOKELACOOP_NAME    "TokelaCoop"
// "TokelaCoop v0.54"
#define TOKELACOOP_TITLE   TOKELACOOP_NAME " v" TOKELACOOP_VERSION

#endif
