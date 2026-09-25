TokelaCoop - co-op mod (antes KenshiCoop / formerly KenshiCoop)
===============================================================
  La versión está en PROVENANCE.json y en el juego: arriba a la izquierda y en
  el título del panel F2 sale "TokelaCoop vX.YY". Los dos, la misma.
  The version is in PROVENANCE.json and in game: "TokelaCoop vX.YY" in the
  top-left corner and on the F2 panel title. Both players: the same one.

INSTALAR (los dos jugadores) - en español
-----------------------------------------
  1. Descomprime el zip entero en cualquier carpeta.
  2. Doble clic en "Instalar TokelaCoop.cmd". Si Windows pregunta, pulsa
     "Ejecutar" / "Más información > Ejecutar de todas formas".
  3. El instalador busca Kenshi, instala RE_Kenshi si falta (abre su instalador
     oficial: pulsa Install), copia el mod y lo activa. Al acabar dice "Listo".
     Si tenías KenshiCoop (el nombre antiguo, hasta la v0.53), lo cambia por
     TokelaCoop y conserva tu coop_config.json.
  Para actualizar, descarga el zip nuevo y repite. Para jugar: mira PLAY abajo
  (el anfitrión pulsa F2 > "Invite a Steam friend").

INSTALL (both players)
----------------------
  1. Extract the whole zip anywhere.
  2. Double-click "Instalar TokelaCoop.cmd" (if Windows asks, choose "Run" /
     "More info > Run anyway").
  3. It finds Kenshi (any Steam library or GOG; it asks if it can't), installs
     RE_Kenshi if missing (it downloads the pinned release from GitHub, checks
     its SHA-256 and opens RE_Kenshi's official installer - click Install),
     copies the mod into <Kenshi>\mods\TokelaCoop and enables it in the mod
     list. It ends with "Done". Run it again to update. If you had KenshiCoop
     (the old name, up to v0.53) it switches it to TokelaCoop and keeps your
     coop_config.json.

  Manual install instead: install RE_Kenshi 0.3.5
  (https://github.com/BFrizzleFoShizzle/RE_Kenshi/releases), copy the
  "TokelaCoop" folder into <Kenshi>\mods\ and enable "TokelaCoop" in the
  launcher's Mods list. Upgrading from KenshiCoop by hand: move
  mods\KenshiCoop\coop_config.json into mods\TokelaCoop\, delete
  mods\KenshiCoop and untick KenshiCoop - with both loaded, TokelaCoop stays
  off and says why.

PREREQUISITES (both players)
----------------------------
  1. Kenshi 1.0.65 or 1.0.68 (Steam or GOG).
  2. RE_Kenshi 0.3.4 or 0.3.5 - the installer takes care of it.
  3. For the Steam transport (recommended): Steam RUNNING and ONLINE on both
     machines. No port forwarding, no IPs, no config editing.
  4. The SAME TokelaCoop version on both machines (the handshake rejects a
     mismatch) - download the same zip.

PLAY (Steam - recommended)
--------------------------
  EASIEST - invite from the panel (new, not yet tested in-game):
  Your friend just needs Kenshi running with the mod enabled (main menu is
  fine). The HOST loads a save, presses F2 and clicks "Invite a Steam
  friend", then clicks "Invite <name>" next to the friend. The friend
  accepts the Steam notification and both sides connect on their own.
  If that doesn't work, use the manual steps below.

  1. Press F2 to open the Co-op panel. It works at the MAIN MENU (before loading
     a game) as well as in-game.
  2. Swap Steam IDs: each player clicks "Copy my Steam ID" and sends it to the
     other (Steam chat, Discord, etc.). When you receive your friend's ID, copy
     it, then click "Paste friend's Steam ID" in your panel. The panel shows the
     ID it captured. (This is per-session - re-paste it if you relaunch Kenshi.)
  3. HOST: load the save you want to play (or start a new game), set Role: HOST,
     leave Transport on STEAM, and toggle Connection to ONLINE.
  4. JOIN: straight from the MAIN MENU - no save needed - set Role: JOIN, leave
     Transport on STEAM, and toggle Connection to ONLINE. The host sends its
     world to you on connect and you load right into it. (You do NOT need the
     host's save beforehand. If you already have an identical copy on disk it is
     used as-is instead of transferring.)
  5. The white status line shows live state, and a status banner in the TOP-LEFT
     corner shows it too - at the main menu as well as in-game, so a joining
     player can watch the transfer before the world loads. Toggle Connection to
     OFFLINE to leave.

PLAY (LAN / direct UDP - advanced)
----------------------------------
  Skip the Steam ID swap. Open <Kenshi>\mods\TokelaCoop\coop_config.json in
  Notepad, set "transport": "udp", and put the host's address in "ip" (and
  "port" if you changed it). In the panel set Transport: UDP, pick Host/Join,
  and go ONLINE. ip/port are re-read whenever you go ONLINE, so no restart is
  needed after an edit.

UNINSTALL
---------
  Delete <Kenshi>\mods\TokelaCoop and remove the "TokelaCoop.mod" line from
  <Kenshi>\data\mods.cfg (or untick it in the launcher). To remove RE_Kenshi,
  run its installer and choose Uninstall.

TROUBLESHOOTING
---------------
  * "The co-op plugin has not started": RE_Kenshi didn't load it. Check
    <Kenshi>\RE_Kenshi_log.txt for 'TokelaCoop'; reinstalling RE_Kenshi
    usually fixes it.
  * No connection (Steam): both Steams must be RUNNING and ONLINE, and each side
    must have Pasted the OTHER player's ID (the panel shows the captured ID -
    confirm it matches). If "Paste friend's Steam ID" says the clipboard wasn't
    a Steam ID, have your friend re-copy theirs with "Copy my Steam ID". Look for
    '[steam] session ... active=1' in <Kenshi>\TokelaCoop_*.log.
  * "Mods: DIFFERENT" on the F2 panel: you and your friend don't have the same
    mods, versions or load order. <Kenshi>\TokelaCoop_mods_diff.txt lists every
    difference; "Copy friend's mod list" copies their load order. The check
    only warns, it never blocks the connection (new, not yet tested in-game).
  * "your friend has another version" on the F2 panel: one player has an
    older/newer build; both should use the same release (a friend on v0.53 or
    older still calls it KenshiCoop).
  * A warning box at start, or "untick KenshiCoop" on the banner: the old
    KenshiCoop is still installed next to TokelaCoop. Run the installer again,
    or untick KenshiCoop in the launcher's Mods tab.
