# KenshiCoop: instalar fácil, conectar fácil y mods (2 jugadores)

> Investigación del 2026-09-23, hecha desde macOS leyendo el repo, GitHub y la web.
> Aquí no se ha compilado ni ejecutado nada. **Sin verificar** marca lo que no se ha podido comprobar.
> El alcance es **host + 1 amigo**. El guard de 2 jugadores ya está en `src/plugin/net/NetLink.cpp:481-488`.

---

## 1. Instalación de hoy, paso a paso (para un amigo no técnico)

### Requisitos

| Pieza | Estado real (sept. 2026) | Fuente |
|---|---|---|
| Kenshi (Steam) | En Steam la versión actual es **1.0.68**. El README pide "1.0.65" (`README.md:52`), y eso confunde. RE_Kenshi está basado en 1.0.65 y, en 1.0.68, "crea un ejecutable compatible" en `<Kenshi>\RE_Kenshi\`. Que KenshiCoop funcione en un Steam 1.0.68 gracias a ese exe: **sin verificar**. | [README de RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi), [Changelog de la wiki](https://kenshi.fandom.com/wiki/Changelog) |
| RE_Kenshi | Se publica en **GitHub Releases** (última: v0.3.5, 2026-08-26, `RE_Kenshi_v0.3.5.zip` de 11,8 MB con instalador `.exe`, y un `_loose.zip` para instalar a mano) y en **Nexus** (mod 847). **No hay versión oficial en Steam Workshop.** Nexus exige una cuenta (gratuita) para descargar. La instalación manual solo vale para 1.0.65: en 1.0.68 hay que usar el instalador. | `gh api repos/BFrizzleFoShizzle/RE_Kenshi/releases`, [Nexus: login para descargar](https://help.nexusmods.com/article/92-im-having-download-issues-what-can-i-do) |
| KenshiLib (va dentro de RE_Kenshi) | El plugin se compila contra **KenshiLib 0.3.0** (deps `e75769b`, CLAUDE.md). RE_Kenshi 0.3.5 trae una KenshiLib más nueva: KenshiExtensionPlugin pide "KenshiLib v0.5.0 (RE_Kenshi v0.3.5)". **Sin verificar** si la DLL compilada contra la 0.3.0 carga con RE_Kenshi 0.3.4 o 0.3.5, porque la 0.4.0 ya quitó `CombatClass.h`. **Es el mayor riesgo de instalación.** | [KEP en Workshop](https://steamcommunity.com/workshop/filedetails/?id=3638161797) |
| Kit KenshiCoop | `KenshiCoop-kit.zip`. El README enlaza a las releases de **upstream** (`README.md:61-62`, `nhoral/KenshiCoop`, v0.51, protocolo 55). El fork `dentroytu/KenshiCoop` tiene **0 releases**. | `gh api repos/dentroytu/KenshiCoop/releases` |
| Steam | Tiene que estar abierto y *online* en los dos PCs (transporte Steam P2P). | `README.md:55-58` |

### Pasos que sigue hoy un amigo

1. Comprobar que Kenshi es la versión de Steam y arrancarlo una vez.
2. Instalar RE_Kenshi: bajar el zip de GitHub (o de Nexus, con cuenta), **extraerlo entero** y ejecutar `RE_Kenshi_vX.exe`. En el menú principal debe aparecer "RE_Kenshi vx.x.x - Kenshi 1.0.x".
3. Bajar `KenshiCoop-kit.zip`, hacer clic derecho > Propiedades > **Desbloquear** y extraerlo (`dist/mod-kit/README.txt`, paso 1).
4. Copiar la carpeta `KenshiCoop` a `<Kenshi>\mods\`, de modo que quede `<Kenshi>\mods\KenshiCoop\KenshiCoop.dll`. El kit trae `KenshiCoop.dll`, `KenshiCoop.mod`, `RE_Kenshi.json` (`{"Plugins":["KenshiCoop.dll"]}`) y `coop_config.json`.
5. Abrir el launcher de Kenshi > **Mods** y **activar KenshiCoop**. RE_Kenshi solo carga los plugins de los mods **activos**: recorre `ou->activeMods` y lee `<ruta del mod>\RE_Kenshi.json` ([RE_Kenshi `Plugins.cpp`, `Plugins::Postload`](https://github.com/BFrizzleFoShizzle/RE_Kenshi/blob/master/Plugins.cpp)). El orden de carga queda en `<Kenshi>\data\mods.cfg`, una línea por mod y en orden ([hilo de Steam](https://steamcommunity.com/app/233860/discussions/0/1739964947811092476/)). KenshiCoop.mod solo añade dos *starts*, así que su posición da igual. Lo que importa es que **los dos jugadores tengan la misma lista y el mismo orden**, y eso no se explica en ningún sitio.
6. Jugar: F2 > "Copy my Steam ID" > mandar el ID por Discord o Steam > el otro hace "Paste friend's Steam ID" > Role HOST/JOIN > Connection ONLINE (`README.md:77-104`).

### Puntos de fricción (todos)

| # | Fricción | Dónde |
|---|---|---|
| F1 | Hay dos webs de descarga distintas (GitHub para el kit; Nexus con cuenta o GitHub para RE_Kenshi). El README solo enlaza Nexus. | `README.md:53`, `README.txt` |
| F2 | Hay gente que instala **KenshiExtensionPlugin** desde Workshop creyendo que es RE_Kenshi, y el F2 no funciona. | [nhoral/KenshiCoop#4](https://github.com/nhoral/KenshiCoop/issues/4) |
| F3 | Se pide "Kenshi 1.0.65", pero Steam instala 1.0.68. | `README.md:52` |
| F4 | "RE_Kenshi 0.3.1+" es un rango abierto, pero el plugin está fijado a KenshiLib 0.3.0 (ver arriba). No hay matriz de versiones probadas. | `README.md:53`, CLAUDE.md |
| F5 | Encontrar la carpeta de Kenshi: el README solo da la ruta por defecto en `C:`. Las bibliotecas de Steam en otros discos son muy comunes. | `README.txt`; `friend_host.ps1:195-196`, `friend_join.ps1:104-105`, `deploy.cmd:13` solo prueban `C:\Program Files*` |
| F6 | Mark-of-the-Web: el paso de "Desbloquear" es manual en el kit de jugador. Solo el kit de pruebas lo automatiza (`kit_preflight.ps1:9-18`). Smart App Control puede bloquear DLLs sin firmar (lo avisa KEP). | `make_mod_kit.ps1:111-113` |
| F7 | Si olvidas activar el mod en el launcher, el plugin no carga y el juego se ve vanilla sin ningún aviso. | `Plugins.cpp` de RE_Kenshi |
| F8 | No se explica ni se comprueba que la lista de mods y el orden sean iguales en los dos PCs. | `grep` en `src/`: no hay ninguna comprobación (§4) |
| F9 | El README apunta a las releases de upstream, no a las del fork. Si el fork cambia `PROTOCOL_VERSION` (hoy 55, `src/netproto/Wire.h:28`), los amigos que bajen el zip de upstream no podrán conectar. | `README.md:61-62` |
| F10 | Intercambio de Steam ID por portapapeles y chat externo. Solo dura la sesión y hay que volver a pegarlo tras reiniciar. | `EngineUi.cpp:258-266`, `EngineUi.cpp:290-320` |
| F11 | Si no coincide el protocolo, el host corta (`NetLink.cpp:471-478`, `enet_peer_disconnect(peer, 0)` sin motivo) y el cliente reintenta cada 2 s sin parar (`NetLink.cpp:422-441`). El error **solo sale en el log**, no en el panel. | `NetLink.cpp` |
| F12 | Se recomienda modo ventana, pero ese aviso solo está en el kit de pruebas. | `kit_preflight.ps1:50-64` |
| F13 | `HOST.cmd`, `JOIN.cmd` y `friend_*.ps1` son del **kit de pruebas remoto** (`make_remote_kit.ps1`), no del kit de jugador. Es fácil confundirlos. | `scripts/` |

---

## 2. Ideas para que instalar sea trivial (ordenadas por impacto/esfuerzo)

| Idea | Esfuerzo | Impacto | Veredicto |
|---|---|---|---|
| **A. Pipeline de release en GitHub Actions** | S | Alto | `build.yml` ya compila `Release` en `windows-2022` y sube las DLLs como artefacto. Faltaría un job `on: push: tags: v*` que ejecute `make_mod_kit.ps1 -SkipBuild`, que ya resuelve `.mod` y `.json` desde `dist\mods` (`make_mod_kit.ps1:52-71`) y genera `PROVENANCE.json` y el SHA-256, y que publique con `gh release create`. Además, cambiar el README para que apunte a las releases del **fork** (arregla F9). Se hace entero en CI. |
| **B. Instalador de un clic** (`Instalar-KenshiCoop.cmd` que llama a un `.ps1`) | M | Muy alto | Pasos: (1) leer `HKCU:\Software\Valve\Steam\SteamPath` (ya lo hace `kit_preflight.ps1:78`) y parsear `steamapps\libraryfolders.vdf` para encontrar la librería que contiene la app `233860` (F5); (2) comprobar `kenshi_x64.exe` (1.0.68 de Steam = 36.718.592 bytes según [zeroit789/kenshi-coop](https://github.com/zeroit789/kenshi-coop), **sin verificar**); (3) detectar RE_Kenshi (`RE_Kenshi.dll` más `Plugin=RE_Kenshi` en `Plugins_x64.cfg`). La comprobación actual acepta `dinput8.dll`, que no tiene nada que ver (`kit_preflight.ps1:30-32`). Si falta RE_Kenshi, **bajar la versión probada desde GitHub con un SHA-256 fijado y lanzar su instalador oficial**, que es necesario en 1.0.68; (4) copiar `mods\KenshiCoop` y quitar el MOTW; (5) **añadir `KenshiCoop.mod` a `data\mods.cfg`**, lo que elimina el paso manual (F7); (6) avisar de Smart App Control y del modo pantalla completa; (7) mostrar "Todo OK" más el Steam ID. Mejor `.cmd` + `.ps1` en texto plano que un `.exe` hecho con ps2exe, porque esos los marcan los antivirus. Se puede escribir y probar con Pester en CI con carpetas falsas, pero la prueba final necesita Windows con Kenshi. |
| **C. Publicar KenshiCoop en Steam Workshop** | M | Muy alto (actualiza solo, así que los dos jugadores tienen siempre la misma versión) | **Sí es viable técnicamente**. RE_Kenshi carga `RE_Kenshi.json` desde `ModInfo::path`, que incluye los mods de Workshop (`ModInfo.h:14-17`: `path` e `isWorkshop`). La config se busca junto a la DLL (`core/Config.cpp:73-83`), no en `mods\`. KenshiExtensionPlugin ya distribuye una DLL por Workshop y tiene 66.542 suscriptores. **Pero RE_Kenshi no está en Workshop**: tiene que tocar la raíz del juego y `Plugins_x64.cfg`, cosa que Workshop no puede hacer, y los "required items" de Workshop solo pueden apuntar a otros items de Workshop. Así que Workshop solo resuelve la mitad KenshiCoop. Riesgos: tener a la vez la copia de Workshop y la de `mods\KenshiCoop` carga el plugin dos veces (el instalador debería detectarlo); los scripts suponen `mods\KenshiCoop` (`friend_host.ps1:208`, `deploy.cmd:25`); subir con el FCS o con steamcmd requiere Windows y cuenta de Steam (no se puede automatizar en CI con Steam Guard, **sin verificar**); hay colecciones con plugins que Steam ha retirado ([ejemplo](https://steamcommunity.com/sharedfiles/filedetails/?id=3697494942), motivo sin verificar). |
| **D. Aviso de actualización contra GitHub Releases** | S-M | Medio | Un hilo en segundo plano con WinHTTP (RE_Kenshi ya lo hace, `WinHttpClient.cpp`) que consulte `api.github.com/repos/dentroytu/KenshiCoop/releases/latest` y muestre "Hay versión nueva" en F2. Mejor todavía: que el instalador (B) sirva también para actualizar. Si se hace (C), esto sobra. |
| **E. Empaquetar RE_Kenshi dentro del kit** | S | Medio | **Licencia: GPL-3.0** (campo `license` del repo en GitHub), así que redistribuirlo está permitido si se incluye la licencia y el acceso al código fuente del tag exacto. Las licencias de sus dependencias (CompressTools, etc.) y los permisos de Nexus están **sin verificar** (Nexus devuelve 403). Aun así, **no lo recomiendo**: en 1.0.68 hay que ejecutar su instalador de todas formas. Es mejor que (B) lo baje de GitHub con el hash fijado. Por cortesía, avisar a BFrizzleFoShizzle. |
| **F. Comprobador del kit de jugador** | S | Medio | Portar `Test-CoopPrereqs` y `Wait-PluginLoaded` (`kit_preflight.ps1`) al kit de jugador como `Comprobar.cmd`. Es un subconjunto de (B). |

---

## 3. Ideas para que conectar sea trivial (1 amigo)

### Qué hay hoy

- **Panel F2** (`game/EngineUi.cpp`): Role, Transport y Connection ONLINE/OFFLINE, más "Copy my Steam ID" y "Paste friend's Steam ID" por portapapeles (`EngineUi.cpp:176-230`, `290-320`, `525-532`). El ID pegado solo vive en memoria (`EngineUi.cpp:262-266`). No hay campos de texto porque las editbox de MyGUI no reciben foco (`EngineUi.h:16-19`).
- **SteamP2P** (`net/SteamP2P.cpp`): obtiene las interfaces por `GetModuleHandleA("steam_api64.dll")` + `GetProcAddress` de la *flat API*, **sin cabeceras del SDK** (`SteamP2P.cpp:263-277`): `SteamUser019` (`:294`) y `SteamNetworking005/006/004` (`:297-306`, "Kenshi's DLL is SteamClient017-era"). Túnel ENet de un solo peer (`g_peer`). La AppID es la de Kenshi (233860, `SteamInvite.cpp:102`).
- **SteamInvite** (`net/SteamInvite.cpp`): **ya implementa** un lobby *friends-only* de **2 plazas** (`:413`), con los datos `kc_protocol` y `kc_game` (`:225-226`), comprobación de versión al entrar (`:277-291`), invitación directa `InviteUserToLobby` (`:436-457`), un selector de amigos en el panel ordenado por "jugando a Kenshi" (`:179-207`), callbacks hechos a mano (`:31-44`) y el bombeo de `SteamAPI_RunCallbacks` desde el hilo principal porque Kenshi no lo hace (`:465-473`). Versiones de interfaz: `SteamMatchMaking009/008/010` y `SteamFriends015/014/017/016` (`:379-383`).
  - **La interfaz de invitar está desconectada**: `beginInvite`, `inviteFriend` y `friendCount` no se llaman desde ningún sitio fuera de SteamInvite (comprobado con `grep`), y `Plugin.cpp:794-796` dice "the outbound invite/picker UI is gone". Solo queda activa la parte **receptora**: si un amigo te manda un "Join Game", `onGameLobbyJoinRequested` (`:247-260`) entra al lobby y llama a `coopUiConnect(false, true, owner)` (`:294-300`). Se inicializa en `Plugin.cpp:2382-2384`.
  - El overlay de Steam (`ActivateGameOverlayInviteDialog`) se sustituyó por el selector propio por un fallo del web-view de Steam (`SteamInvite.h:44-46`).
  - Arranque en frío: el launcher de Kenshi no reenvía `+connect_lobby` (`SteamInvite.h:10-12`), así que los dos tienen que tener ya el juego abierto (vale con estar en el menú principal).
- **Reconexión**: el cliente reintenta cada 2 s mientras la red sigue activa (`NetLink.cpp:422-441`), pero no recuerda a quién se conectó si reinicias Kenshi. Rich presence: no existe (0 resultados de `grep`).

### Propuestas

| Propuesta | Esfuerzo | Impacto | Notas |
|---|---|---|---|
| **1. Botón "Invitar amigo de Steam" en F2** (host) | S-M | **Muy alto** | Volver a conectar lo que ya existe: al pulsarlo se hace `beginInvite()`, que crea el lobby, y se pinta en el panel la lista de amigos (`friendName` y `friendState`) con botones que llaman a `inviteFriend(id)`. El amigo recibe la notificación de Steam, pulsa y entra solo como JOIN: no pega nada ni toca el panel. Al pasar a ONLINE como host, crear también el lobby: como es *friends-only*, es "Joinable by friends and invitees" ([ISteamMatchmaking](https://partner.steamgames.com/doc/api/ISteamMatchmaking)), así que el amigo también puede hacer **"Unirse a la partida" desde la lista de amigos de Steam** sin invitación (lo recibe `GameLobbyJoinRequested`; **sin verificar** en Kenshi). Límite: DatapanelGUI solo tiene botones, así que la lista va paginada (6-8 amigos, primero los que juegan a Kenshi). Hay que verificar por qué se quitó (¿ruido? ¿algún fallo?). No hay historial útil: todo entra en el commit `0513677`. |
| **2. Mensajes claros de versión** | S | Alto | Desconectar con un código de motivo (`enet_peer_disconnect(peer, 1=PROTOCOL, 2=MODS, 3=GAMEVER)`). El cliente lee `ev.data` en `DISCONNECT`, **deja de reintentar** y lo muestra en el panel y el banner: "El host tiene KenshiCoop v0.52 y tú v0.51: actualizad los dos". En el lobby ya existe el mismo mensaje (`SteamInvite.cpp:283-289`). Añadir `kc_mods` (hash de §4) y `kc_build` al lobby. |
| **3. "Reconectar con <nombre>"** | S-M | Medio-alto | Guardar en `mods\KenshiCoop\coop_last.json` el último SteamID del par y el rol (nada sensible; hoy se descarta a propósito, `EngineUi.cpp:262-266`, así que es una decisión de producto). En el menú principal, un botón "Reconectar con Pepe" (el nombre sale de `GetFriendPersonaName`). El host vuelve a abrir el lobby al cargar la partida co-op. |
| **4. Rich presence** | S | Medio | `SteamAPI_ISteamFriends_SetRichPresence("status", "KenshiCoop: esperando amigo")` y `"connect"` ([ISteamFriends](https://partner.steamgames.com/doc/api/ISteamFriends): "connect" es "the command-line for how a friend can connect"). `steam_display` necesita tokens de localización subidos por el desarrollador (Lo-Fi), así que no se puede usar. Que el export exista en el `steam_api64.dll` de Kenshi está **sin verificar**: hay que resolverlo con `GetProcAddress` y degradar sin él. Aporta poco sobre el lobby (1). |
| **5. Código de unión** (p. ej. `KC-7QX2M`) | M | Bajo-medio | El panel no tiene campo de texto, así que el código habría que pegarlo igual que el ID. Solo compensa para jugar **sin ser amigos en Steam**: lobby *public* + `AddRequestLobbyListStringFilter("kc_code", …)`. Para 2 amigos, (1) es mejor. |
| **6. Auto-rol** | S | Medio | Si llega un "Join Game", el rol ya se pone solo (`coopUiConnect(false,…)`). Con (1), el panel del JOIN deja de hacer falta. Queda poner el rol HOST por defecto si hay una partida cargada y JOIN si estás en el menú. |

---

## 4. Mods populares y compatibilidad co-op

### ¿El código comprueba la lista de mods? **No.**

`grep -rniE "mod ?list|load ?order|mods\.cfg|activeMods"` sobre `src/` no encuentra ninguna comprobación. `ContentHash.h` es el hash de inventario, no de mods. El handshake solo compara `PROTOCOL_VERSION` (`Wire.h:133-143`, `NetLink.cpp:471`, `:509`). La transferencia de partida (`sync/SaveXfer.cpp`) copia la partida, pero no los mods.

Consecuencia (hipótesis **sin verificar**): si al JOIN le falta un mod del host, las entidades o ítems de ese mod no se pueden crear en su lado, así que habrá squads o ítems invisibles y *hands* que no resuelven. Esto encaja con los informes de "mundo vacío" o "faltan personajes" ([#83](https://github.com/nhoral/KenshiCoop/issues/83), [#86](https://github.com/nhoral/KenshiCoop/issues/86)), aunque allí no se menciona ningún mod.

### Propuesta de comprobación (2 jugadores)

1. **Datos**: KenshiLib expone `GameWorld::activeMods` (`lektor<ModInfo*>`, `third_party/KenshiLib_deps/KenshiLib/Include/kenshi/GameWorld.h:110`) con `name`, `file`, `path`, `isWorkshop` y `header.version` (`ModInfo.h:14-20`, `GameData.h:31-33`). Ya está lleno en el menú principal, porque RE_Kenshi lo usa para cargar plugins.
2. **Firma**: una lista ordenada de `file|header.version|tamaño del .mod`, más el FNV-1a de la lista entera (`fnv1aInit` y `fnv1aUpdate` ya existen en `netproto/ContentHash.h:79`). Para los mods de Workshop, sacar el ID de Workshop de `path` (`...\workshop\content\233860\<id>`).
3. **Wire**: un nuevo `PKT_MODLIST` (cliente → host tras WELCOME, y host → cliente si falla) con el hash y la lista. Subir `PROTOCOL_VERSION` (regla de CLAUDE.md) y fijarlo en `prototest`. Si hay diferencia, el host responde con su lista y desconecta con el motivo `MODS` (§3.2).
4. **Mensaje** (panel, banner y `KenshiCoop_mods_diff.txt`), por ejemplo:
   > **Los mods no coinciden con los del host.** Te falta: `Dark UI.mod` (Workshop 12345). Te sobra: `Living World.mod`. Orden distinto a partir de la posición 7. Cierra Kenshi, ajusta los mods en el launcher y vuelve a entrar.
   Añadir un botón "Copiar lista del host" que ponga en el portapapeles el contenido en formato `mods.cfg`. El instalador (§2.B) podría aplicarla y abrir `steam://url/CommunityFilePage/<id>` para cada mod de Workshop que falte.
5. **Lobby**: publicar `kc_mods=<hash>` para que el JOIN vea el aviso antes de conectar (mismo patrón que `SteamInvite.cpp:225`).
6. **Política**: por defecto, estricta. Opción `"modCheck": "warn"` en `coop_config.json` para quien sepa que su diferencia es solo cosmética.

### Clasificación de mods populares

Top de Workshop por suscriptores ([listado](https://steamcommunity.com/workshop/browse/?appid=233860&browsesort=totaluniquesubscribers&section=readytouseitems)) y plugins de RE_Kenshi en Nexus/Workshop. La clasificación es mía, a partir de lo que hace cada mod. **Nada de esto se ha probado en co-op.**

| Categoría | Mods | Por qué |
|---|---|---|
| **Seguros (solo aspecto o UI; en teoría podrían diferir entre jugadores, pero la regla simple es tener la misma lista)** | Dark UI, Compressed Textures Project, Nice Map, Reduced Weather Effects, Minor Mesh Fixes, Fixing Clipping Issues, More Names! | Texturas, mallas o UI. No añaden objetos de juego que se repliquen. |
| **Seguros si son idénticos en los dos (datos FCS)** | 256 Recruitment Limit, Weight Bench, Wooden Dexterity Training Dummy, Copper Ore Drills, Better Crop Fences, Slopeless, New Weapons Dissemination, Enhanced Shopping Economy / shops have more items +, Let's Talk, Recruitable Prisoners (diálogos; el reclutamiento pasa por `recruit_hook`) | Solo datos. Si la lista y el orden coinciden, el host es la autoridad del mundo y todo cuadra. |
| **Arriesgados (lógica de NPC, spawns, IA, combate)** | Reactive World, Living World, Cannibals Expanded, Kenshi: Genesis (overhaul), More Combat Animation, Animation Overhaul, Martial Arts Fast Dodger, NPC enjoys more shopping, TameBeasties | Cambian las poblaciones, el comportamiento o la duración de las animaciones de combate. Más carga de replicación y más desincronizaciones posibles, aunque sean idénticos en los dos lados. |
| **Arriesgados (otros plugins de RE_Kenshi/KenshiLib)** | **KenshiExtensionPlugin** (66k suscriptores; engancha inventario, `PlatoonEx`, personaje, ítems e investigación, [repo](https://github.com/Lucius64/KenshiExtensionPlugin)), Extra Inventory Sections ([Nexus 1838](https://www.nexusmods.com/kenshi/mods/1838)), SentientSands (sin verificar), otros mods multijugador (Kenshi-Online, KServerMod) | KenshiCoop engancha `mainLoop_GPUSensitiveStuff`, `SaveManager::save/load`, `Character::periodicUpdate/hitByMeleeAttack`, `Inventory::buyItem/tryAddItem/dropItem`, reclutamiento, edificios y velocidad y pausa (`EngineInternal.cpp:2092-2245`, `EngineCharState.cpp:789-799`, `Plugin.cpp:1924`). Si otro plugin engancha lo mismo, las cadenas de hooks o el inventario pueden divergir. Los mods multijugador son **incompatibles**. |
| **A vigilar** | Las funciones propias de RE_Kenshi (velocidad de juego personalizada, freecam) | Pueden chocar con el hook de `setGameSpeed` (sin verificar). |

### "Co-op pack" recomendado (colección de Workshop)

Empezar mínimo y ampliar solo con lo que se haya probado: **KenshiCoop + Dark UI + Compressed Textures Project + Nice Map + 256 Recruitment Limit + Recruitable Prisoners**. Publicarlo como **colección de Workshop** para que el amigo se suscriba con un clic (esfuerzo S) y documentar el orden en el README. El orden de base que recomiendan las guías de la comunidad es: primero los fixes y overhauls, luego los datos y al final la UI y las texturas ([guía de load order](https://steamcommunity.com/sharedfiles/filedetails/?id=1850250979), sin verificar en detalle).

---

## 5. Roadmap priorizado (2 jugadores, top 8)

| # | Tarea | Esfuerzo | Impacto | ¿Sin Windows? |
|---|---|---|---|---|
| 1 | **Matriz de compatibilidad probada**: Kenshi 1.0.68 de Steam + RE_Kenshi 0.3.5 (KenshiLib ≥0.5) frente a la DLL compilada con KenshiLib 0.3.0. Si no carga, subir `KENSHILIB_DEPS_REF` y arreglar la compilación. Fijar en el README "probado con X/Y". | S-M | Crítico | La prueba necesita Windows con Kenshi. El cambio de deps se puede hacer en CI. |
| 2 | **Pipeline de release** con tag, zip, SHA-256 y PROVENANCE en el fork, y el README apuntando a sus releases (§2.A). | S | Alto | Sí, solo CI. |
| 3 | **Errores de versión visibles** (código de motivo al desconectar, dejar de reintentar y mostrarlo en F2 y el banner), más `kc_build` en el lobby (§3.2). | S | Alto | Compila en CI. La interfaz se prueba en Windows. |
| 4 | **Comprobación de lista de mods** con mensaje claro y "Copiar lista del host" (§4). | M | Alto | Wire y prototest en CI. Leer `activeMods` exige Windows con Kenshi. |
| 5 | **Botón "Invitar amigo" / Unirse desde Steam**, reactivando `SteamInvite` (§3.1). | S-M | Muy alto | Compila en CI. Hacen falta 2 PCs con Kenshi y 2 cuentas de Steam. |
| 6 | **Instalador de un clic** (`.cmd` + `.ps1`): ruta de Steam y librerías, RE_Kenshi bajado de GitHub con hash fijado, copia, alta en `mods.cfg`, desbloqueo y verificación (§2.B). | M | Muy alto | Lógica y Pester en el runner Windows de CI. La prueba final con Kenshi real. |
| 7 | **"Reconectar con <amigo>"**: recordar el último par y que el host reabra el lobby (§3.3). | S-M | Medio-alto | Compila en CI. Prueba en Windows. |
| 8 | **Steam Workshop**: publicar KenshiCoop (actualiza solo y deja a los dos en la misma versión) más la colección "co-op pack". El instalador se reduce a RE_Kenshi y la comprobación (§2.C). | M | Muy alto | No: el FCS o steamcmd necesitan Windows y cuenta de Steam. |

### Fuentes externas

- RE_Kenshi: [GitHub](https://github.com/BFrizzleFoShizzle/RE_Kenshi), [Releases](https://github.com/BFrizzleFoShizzle/RE_Kenshi/releases), [Plugins.cpp](https://github.com/BFrizzleFoShizzle/RE_Kenshi/blob/master/Plugins.cpp), [Nexus 847](https://www.nexusmods.com/kenshi/mods/847) (devuelve 403: permisos sin verificar)
- KenshiLib: [KenshiReclaimer/KenshiLib releases](https://github.com/KenshiReclaimer/KenshiLib/releases), [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps)
- [KenshiExtensionPlugin en Workshop](https://steamcommunity.com/workshop/filedetails/?id=3638161797), [repo](https://github.com/Lucius64/KenshiExtensionPlugin)
- Steamworks: [ISteamMatchmaking](https://partner.steamgames.com/doc/api/ISteamMatchmaking), [ISteamFriends](https://partner.steamgames.com/doc/api/ISteamFriends), [Matchmaking](https://partner.steamgames.com/doc/features/multiplayer/matchmaking)
- Nexus: [ayuda de descargas](https://help.nexusmods.com/article/92-im-having-download-issues-what-can-i-do)
- Kenshi: [Changelog](https://kenshi.fandom.com/wiki/Changelog), [mods.cfg](https://steamcommunity.com/app/233860/discussions/0/1739964947811092476/), [Workshop más suscritos](https://steamcommunity.com/workshop/browse/?appid=233860&browsesort=totaluniquesubscribers&section=readytouseitems)
- Issues de upstream: [#4](https://github.com/nhoral/KenshiCoop/issues/4), [#83](https://github.com/nhoral/KenshiCoop/issues/83), [#86](https://github.com/nhoral/KenshiCoop/issues/86)
