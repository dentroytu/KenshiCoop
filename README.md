# TokelaCoop

**Current version: v0.54.** Up to v0.53 this project was called **KenshiCoop**.
TokelaCoop is a modified fork of [nhoral/KenshiCoop](https://github.com/nhoral/KenshiCoop).
In game, the top-left banner and the F2 panel title show the name and the
version ("TokelaCoop v0.54"): check that you and your friend see the same one.

Setup + Demo (upstream KenshiCoop): [https://www.youtube.com/watch?v=OqwVRRZEYGM](https://www.youtube.com/watch?v=OqwVRRZEYGM)

Experimental **co-op multiplayer for [Kenshi](https://lofigames.com/)**, built as an
[RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) /
[KenshiLib](https://github.com/BFrizzleFoShizzle/KenshiLib) plugin.

One player hosts their world; a friend connects (LAN, direct UDP, or Steam P2P)
and plays their own squad inside it. The plugin replicates squads, NPCs, combat,
inventory and equipment, direct trades between the players' squads, items
dropped on the ground (both directions), base building and container contents,
one shared money pool, game speed, and more - and saves are coordinated: any save
either player makes becomes one shared save, streamed to both machines
automatically.

> **Status: work in progress.** This is a hobby project under active
> development. Expect rough edges, desyncs, and crashes. Two players is the
> current design target.

## How it works

- `TokelaCoop.dll` is loaded into the game by RE_Kenshi. It hooks the engine via
  KenshiLib and drives all game mutation on the main thread.
- Networking is [ENet](https://github.com/lsalzman/enet) over UDP, with an
  optional Steam P2P tunnel (no port forwarding needed).
- The host is authoritative for the world; each client is authoritative for its
  own squad. See `docs/API_REFERENCE.md` for the full engine-control surface and
  wire protocol.

```
src/plugin/       The TokelaCoop plugin (net, sync/replication, engine facade, scenarios)
src/netproto/     Shared wire-protocol headers (plain C++03, compiled by everything)
src/nettest/      Standalone ENet console app (transport de-risking)
src/netsim/       Protocol simulator
src/prototest/    Wire-protocol unit tests
src/tunneltest/   Steam-tunnel socket-hook tests
scripts/          Build, deploy, session, and automated-test tooling (PowerShell)
docs/             Build guide, engine/API reference, replication pitfalls
third_party/      ENet patches, VC10 compat shim (deps are fetched, not committed)
```

## Try it (play with a friend)

Two players, two machines. You configure the session **inside the game** with
an in-game panel (press **F2**) - you swap Steam IDs by clipboard right in the
panel, so there's no config file to edit and no launcher scripts to run. (A tiny
`coop_config.json` is only needed for LAN / direct-UDP games.)

### Before you start (both players)

1. **Kenshi 1.0.65 or 1.0.68** (Steam or GOG).
2. **Steam running and online** on both machines. That's the whole network
   setup: the connection is Steam P2P, so there's no port forwarding, no
   router configuration, and no IP addresses. (A direct-UDP mode is also
   available for LAN / port-forwarded games.)
3. **The same TokelaCoop release** on both machines. The handshake rejects a
   different network protocol, but two releases can share one, so compare the
   "TokelaCoop vX.YY" both of you see on the banner or the F2 panel.

### 1. Install (one click)

Grab `TokelaCoop-kit.zip` from the
[latest release of this fork](https://github.com/dentroytu/TokelaCoop/releases/latest),
extract it anywhere and double-click **`Instalar TokelaCoop.cmd`** (if Windows
asks, choose *Run* / *More info > Run anyway*). The installer:

- finds Kenshi in any Steam library or GOG (and asks for the folder if it
  can't);
- installs **[RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi)** if
  it's missing. It downloads the pinned 0.3.5 release, checks its SHA-256 and
  opens RE_Kenshi's official installer with your Kenshi folder on the
  clipboard. Click *Install*: that installer is required on Kenshi 1.0.68;
- copies the mod into `<Kenshi>\mods\TokelaCoop` (keeping your
  `coop_config.json`) and enables it in `data\mods.cfg`;
- **upgrading from KenshiCoop (v0.53 or older)**: carries your
  `mods\KenshiCoop\coop_config.json` over, swaps `KenshiCoop.mod` for
  `TokelaCoop.mod` in `data\mods.cfg` (same place in the load order) and removes
  `mods\KenshiCoop`, so the old and new plugin never load together.

Run it again to update. The installer is plain PowerShell
(`installer\Install-TokelaCoop.ps1`), so you can read what it does. It is new
in this fork and not yet tested on a real install.

<details><summary>Manual install</summary>

Install RE_Kenshi 0.3.5 with its own installer, copy the kit's `TokelaCoop`
folder into `<Kenshi>\mods\`, then enable **TokelaCoop** in the launcher's
Mods list. If you had **KenshiCoop** (v0.53 or older), move your
`mods\KenshiCoop\coop_config.json` into `mods\TokelaCoop\`, then delete
`mods\KenshiCoop` and untick KenshiCoop: with both loaded, TokelaCoop stays off
and tells you why.
</details>

### 2. Connect in-game (press F2)

The Co-op panel works at the **main menu** (before you load a game) as well as
in-game, so the joining player doesn't need to load anything first.

**Easiest: invite from the panel (new in this fork, not yet tested in-game).**
Your friend just needs Kenshi running with the mod enabled (the main menu is
fine). You load your save, press **F2** and click **"Invite a Steam friend"**.
Then click **"Invite <name>"** next to your friend. Your friend accepts the
Steam notification, and both sides connect on their own. You host, and nobody
copies an ID. If that doesn't work, use the manual steps below.

1. Press **F2** to open the Co-op panel.
2. **Swap Steam IDs.** Each player clicks **"Copy my Steam ID"** and sends it to
   the other (Steam chat, Discord, ...). When you receive your friend's ID, copy
   it, then click **"Paste friend's Steam ID"** - the panel shows the ID it
   captured. This is per-session (nothing is written to disk), so re-paste it if
   you relaunch Kenshi.
3. Leave **Transport** on **STEAM**.
4. **Host:** load the save you want to play, or start a new game - pick
   **TokelaCoop (Wanderer x2)** from the start list for a ready-made two-squad
   co-op start (see below). Then set **Role: HOST** and toggle **Connection** to
   **ONLINE**.
5. **Join:** straight from the **main menu** - no save needed - set
   **Role: JOIN** and toggle **Connection** to **ONLINE**. The host streams its
   world to you on connect and you load right into it. (If you already have an
   identical copy of the host's save on disk, it's used as-is instead of
   transferring.)
6. The white status line shows live state, and a status banner in the **top-left
   corner** shows it too - at the main menu as well as in-game, so a joining
   player can watch the transfer before the world loads. Toggle **Connection** to
   **OFFLINE** to leave.

**LAN / direct-UDP (advanced):** skip the Steam ID swap. Open
`<Kenshi>\mods\TokelaCoop\coop_config.json`, set `"transport": "udp"`, and put
the host's address in `"ip"` / `"port"`. Then in the panel set **Transport: UDP**
and go ONLINE. The `ip`/`port` are re-read whenever you go ONLINE, so no restart
is needed after an edit.

### Good to know

- **You each control your own squad.** With one squad tab per player, the host
  runs squad 1 and the joining player squad 2. Your friend's squad is visible
  and synced on your screen, but answers only to them. If your save has only
  one squad, move some units into a second squad tab in-game to give them a crew.
- **Two-player starts included.** The TokelaCoop mod ships two game starts (New
  Game -> pick one from the list), both the vanilla Wanderer start with two
  wanderers already split into separate squads, so the host gets squad 1 and the
  joining player squad 2 with no manual tab-splitting:
  - **"TokelaCoop (Wanderer x2)"** - the plain version, vanilla in every other
    way. Authored by [zeroit789](https://github.com/zeroit789).
  - **"TokelaCoop+ (Wanderer x2)"** - the same start with **500,000 cats** and both
    characters at **50 in every stat**, for skipping the early grind. Kenshi has
    a single player wallet and co-op shares it, so the 500,000 is the pair's
    combined purse, not 500,000 each. The stats are a floor applied when the
    world loads, so training past 50 sticks normally.
- **The joining player doesn't need the host's save.** The host picks the save
  (or starts a new game); when the join connects from the menu, the host's world
  is streamed over automatically. Already having an identical copy on disk just
  skips the transfer.
- **Saving just works.** Any save either player makes during a session becomes
  one shared save on both machines, streamed to the other side automatically.
  To resume next time, the host loads that save and goes online, and the join
  can reconnect straight from the main menu again.

### If something goes wrong

- **"The co-op plugin has not started"** (no "TokelaCoop vX.YY" in the top-left
  corner) - RE_Kenshi didn't load it. Check
  `<Kenshi>\RE_Kenshi_log.txt` for `TokelaCoop`; reinstalling
  [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847) usually fixes it.
- **"untick KenshiCoop" on the banner, or a warning box at start** - the old
  KenshiCoop is still installed next to TokelaCoop. Run the installer again or
  untick KenshiCoop in the launcher's Mods tab.
- **No connection (Steam)** - both Steams must be online (not offline mode), and
  each side must have **Pasted** the *other* player's ID (the panel shows the
  captured ID - confirm it matches). If "Paste friend's Steam ID" reports the
  clipboard wasn't a Steam ID, have your friend re-copy theirs. Look for
  `[steam] session ... active=1` in `<Kenshi>\TokelaCoop_*.log`.
- **"Mods: DIFFERENT" on the F2 panel** (or "mods differ" on the banner) - you
  and your friend don't have the same mods, versions or load order. Missing
  entities and items are the usual symptom. `<Kenshi>\TokelaCoop_mods_diff.txt`
  lists every difference, and **"Copy friend's mod list"** copies their load
  order to paste into `data\mods.cfg` or match in the launcher. The check only
  warns, it never blocks the connection (new in this fork, not yet tested
  in-game).
- **"your friend has another version" on the F2 panel** - one of you has an
  older build; both players should re-install from the same release. A friend on
  v0.53 or older still has it under its old name, KenshiCoop.

The kit's `README.txt` has the full setup + troubleshooting list.

## Building

The plugin must be compiled with the **Visual C++ 2010 (v100) x64 toolset** (a
KenshiLib requirement). Full toolchain setup, gotchas, and install steps are in
[docs/BUILD_SETUP.md](docs/BUILD_SETUP.md). Short version, once prerequisites
are in place:

```bash
cmd //c scripts/build_plugin.cmd
```

Dependencies are fetched, not committed:

- KenshiLib + precompiled libs: clone
  [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps)
  into `third_party/KenshiLib_deps/`
- ENet: clone [lsalzman/enet](https://github.com/lsalzman/enet) into
  `third_party/enet/enet/` and apply the patches in `third_party/enet/patches/`
  (see `third_party/enet/README.md`)

## Development and testing

`scripts/` contains an automated two-client test harness: `dev_cycle.ps1`
rebuilds, deploys to two local installs, launches host + join, runs a named
scenario, and produces a numeric PASS/FAIL verdict from the two logs.
`regress.ps1` runs the scenario regression suite. See
[docs/BUILD_SETUP.md](docs/BUILD_SETUP.md) Parts D-E for details.

## Credits

- [BFrizzleFoShizzle](https://github.com/BFrizzleFoShizzle) - RE_Kenshi and
  KenshiLib, which make plugins like this possible
- [lsalzman/enet](https://github.com/lsalzman/enet) - UDP networking library
- [Nhoral](https://github.com/nhoral) - [KenshiCoop](https://github.com/nhoral/KenshiCoop),
  the original project this fork is built on
- [zeroit789](https://github.com/zeroit789) - the original "Multiplayer (Wanderer x2)"
  co-op game start, now "TokelaCoop (Wanderer x2)"
  ([#15](https://github.com/nhoral/KenshiCoop/pull/15))
- Lo-Fi Games - Kenshi

## License

[AGPL-3.0](LICENSE). KenshiLib and RE_Kenshi are GPLv3; this plugin links
KenshiLib under GPLv3 section 13 (GPL/AGPL combination). Not affiliated with
Lo-Fi Games. Non-commercial fan project.
