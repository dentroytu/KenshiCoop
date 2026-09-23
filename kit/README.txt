KenshiCoop - co-op mod
======================

INSTALAR (los dos jugadores) - en español
-----------------------------------------
  1. Descomprime el zip entero en cualquier carpeta.
  2. Doble clic en "Instalar KenshiCoop.cmd". Si Windows pregunta, pulsa
     "Ejecutar" / "Más información > Ejecutar de todas formas".
  3. El instalador busca Kenshi, instala RE_Kenshi si falta (abre su instalador
     oficial: pulsa Install), copia el mod y lo activa. Al acabar dice "Listo".
  Para actualizar, descarga el zip nuevo y repite. Para jugar: mira PLAY abajo
  (el anfitrión pulsa F2 > "Invite a Steam friend").

INSTALL (both players)
----------------------
  1. Extract the whole zip anywhere.
  2. Double-click "Instalar KenshiCoop.cmd" (if Windows asks, choose "Run" /
     "More info > Run anyway").
  3. It finds Kenshi (any Steam library or GOG; it asks if it can't), installs
     RE_Kenshi if missing (it downloads the pinned release from GitHub, checks
     its SHA-256 and opens RE_Kenshi's official installer - click Install),
     copies the mod into <Kenshi>\mods\KenshiCoop and enables it in the mod
     list. It ends with "Done". Run it again to update.

  Manual install instead: install RE_Kenshi 0.3.5
  (https://github.com/BFrizzleFoShizzle/RE_Kenshi/releases), copy the
  "KenshiCoop" folder into <Kenshi>\mods\ and enable "KenshiCoop" in the
  launcher's Mods list.

PREREQUISITES (both players)
----------------------------
  1. Kenshi 1.0.65 or 1.0.68 (Steam or GOG).
  2. RE_Kenshi 0.3.4 or 0.3.5 - the installer takes care of it.
  3. For the Steam transport (recommended): Steam RUNNING and ONLINE on both
     machines. No port forwarding, no IPs, no config editing.
  4. The SAME KenshiCoop version on both machines (the handshake rejects a
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
  Skip the Steam ID swap. Open <Kenshi>\mods\KenshiCoop\coop_config.json in
  Notepad, set "transport": "udp", and put the host's address in "ip" (and
  "port" if you changed it). In the panel set Transport: UDP, pick Host/Join,
  and go ONLINE. ip/port are re-read whenever you go ONLINE, so no restart is
  needed after an edit.

UNINSTALL
---------
  Delete <Kenshi>\mods\KenshiCoop and remove the "KenshiCoop.mod" line from
  <Kenshi>\data\mods.cfg (or untick it in the launcher). To remove RE_Kenshi,
  run its installer and choose Uninstall.

TROUBLESHOOTING
---------------
  * "The co-op plugin has not started": RE_Kenshi didn't load it. Check
    <Kenshi>\RE_Kenshi_log.txt for 'KenshiCoop'; reinstalling RE_Kenshi
    usually fixes it.
  * No connection (Steam): both Steams must be RUNNING and ONLINE, and each side
    must have Pasted the OTHER player's ID (the panel shows the captured ID -
    confirm it matches). If "Paste friend's Steam ID" says the clipboard wasn't
    a Steam ID, have your friend re-copy theirs with "Copy my Steam ID". Look for
    '[steam] session ... active=1' in <Kenshi>\KenshiCoop_*.log.
  * "protocol mismatch": one player has an older/newer build; both should use
    the same release.
