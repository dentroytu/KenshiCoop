# CLAUDE.md — KenshiCoop (fork de dentroytu)

Mod cooperativo de Kenshi, fork de `nhoral/KenshiCoop` (base: v0.51, commit `5a761e1`).
Remotes: `origin` = dentroytu/KenshiCoop, `upstream` = nhoral/KenshiCoop.

> **Estado del borrador (2026-09-23):** redactado leyendo el repo desde macOS.
> Nada de lo que sigue se ha compilado ni ejecutado todavía. Marcar como
> verificado cada paso cuando funcione en Windows.

## Objetivo del fork

**Co-op de hasta 4 jugadores (host + 3) y que conectarse sea fácil.**
Upstream está diseñado para 2. Puntos donde hoy asume un único par (hallados leyendo el código, sin verificar a fondo):

- `net/SteamP2P.cpp`: el túnel Steam tiene un solo peer (`g_peer`), un socket falso y una dirección fabricada.
- Panel F2: pega un único Steam ID de amigo.
- Squads: el host tiene el squad 1 y el join el squad 2. Las starts "Wanderer x2" traen dos squads.
- `net/NetLink.cpp`: `enet_host_create(..., 8 /*peers*/ ...)` ya admite varios peers en UDP,
  pero habrá que revisar la lógica de replicación y autoridad, que puede asumir un solo remoto.
- Harness: exactamente dos instalaciones (host + `Kenshi-Join`).
- Referencia: `LogoutUser/KenshiCoopTrio` intentó 3 jugadores con relay en el host (sin compilar ni probar).

## Plataforma

Solo Windows x64. El plugin **debe** compilarse con el toolset **VC++ 2010 (v100) x64**
(requisito de KenshiLib: ABI/layout de structs). No vale un compilador moderno ni
funciona desde macOS/Linux.

## Dependencias de toolchain (según `scripts/build_plugin.cmd`)

- Windows SDK 7.1 + VC2010 SP1 compiler update (KB2519277) → compilador v100 x64.
  Rutas fijas en el script:
  - `C:\Program Files (x86)\Microsoft Visual Studio 10.0`
  - `C:\Program Files\Microsoft SDKs\Windows\v7.1`
- VS2022 Build Tools (solo para `MSBuild.exe`; se localiza con `vswhere`).
- Git. PowerShell 5.1 (el que trae Windows).
- CMake solo para `src/nettest` (no hace falta para el plugin).

`docs/BUILD_SETUP.md` es un stub que apunta a `resources/BUILD_SETUP.md`, que está en
`.gitignore` y **no existe en el repo**. La guía real de toolchain no está publicada.

## Dependencias de código (no versionadas, van en `third_party/`)

```bat
git lfs install
git clone https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps third_party\KenshiLib_deps
:: los .lib y boost.zip están en Git LFS; Setup.bat descomprime boost y fija KENSHILIB_DIR/BOOST_INCLUDE_PATH (pide admin)
cd third_party\KenshiLib_deps && git lfs pull && Setup.bat && cd ..\..
git clone --branch v1.3.18 https://github.com/lsalzman/enet third_party\enet\enet
git apply third_party\enet\patches\0001-enet-c89-for-loops.patch
git apply third_party\enet\patches\0002-enet-socket-hooks.patch
```

- Los parches se aplican desde la raíz del repo. El 0001 hace ENet 1.3.18 compatible con C89 (v100).
- `third_party/vc10_compat/` es un shim versionado (`ammintrin.h`, `OgreConfig.h`, `OgrePlatformInformation.h`).
- Las fuentes de ENet se compilan directamente dentro de `KenshiCoop.vcxproj`.

## CI (compilar sin Windows)

`.github/workflows/build.yml` compila la DLL (Harness y Release) y ejecuta `prototest` en `windows-2022`
en cada push. Instala el toolchain v100 igual que `tools/*.ps1`. Los artefactos son las DLLs.
Si solo tienes el Mac, haz push y descarga la DLL del run:
`gh run download --repo dentroytu/KenshiCoop -n KenshiCoop-<sha>`.

## Compilar

```bat
scripts\build_plugin.cmd            :: Harness (por defecto): DLL de test con scenario runner
scripts\build_plugin.cmd Release    :: DLL para jugadores (sin código de escenarios)
scripts\build_plugin.cmd Debug
scripts\build_prototest.cmd         :: dist\prototest.exe: tests unitarios del wire protocol
```

Salida: `src\plugin\x64\<Config>\KenshiCoop.dll`.

## Desplegar

```bat
scripts\deploy.cmd ["C:\ruta\a\Kenshi"] [Harness|Release|Debug]
```

Copia la DLL, `RE_Kenshi.json` y `KenshiCoop.mod` a `<Kenshi>\mods\KenshiCoop\`.
Por defecto usa la ruta de Steam. Requisitos en el juego: Kenshi 1.0.65 (Steam) + RE_Kenshi 0.3.1+.

## Testear (harness de dos clientes en una sola máquina)

- **Host:** la instalación de Steam, `C:\Program Files (x86)\Steam\steamapps\common\Kenshi`
  (fija en `dev_cycle.ps1` y en `deploy.cmd` por defecto).
- **Join:** una copia independiente en `%USERPROFILE%\Kenshi-Join`, creada con
  `scripts\setup_join_install.cmd`. Es seguro repetirlo: conserva los `save/` y la configuración del join.
- Los clientes se configuran por variables de entorno `KENSHICOOP_*` (MODE, IP, PORT,
  SAVE, TEST_SECONDS, SCENARIO, LOG…). Ver `docs/API_REFERENCE.md` §14.

```powershell
# ciclo completo: matar Kenshi -> build -> deploy -> host+join -> veredicto (exit 0 = PASS)
powershell -ExecutionPolicy Bypass -File scripts\dev_cycle.ps1 -Save "<save>" -Scenario coop_presence -Sync
# suite de regresión (smoke = un escenario por pipeline)
powershell -ExecutionPolicy Bypass -File scripts\regress.ps1
powershell -ExecutionPolicy Bypass -File scripts\regress.ps1 -Tier full -SkipBuild
```

- `-Sync` copia los saves del host a la instalación del join (ambos deben cargar el mismo save).
- Escenarios y oráculos: `scripts/scenarios.psd1` + `scripts/CoopOracles.psm1` + `scripts/oracles/`.
- Logs, capturas y `verdict.json` en `tools/test-runs/<stamp>/`.
- En el juego: `<Kenshi>\KenshiCoop_host.log` / `_join.log` y `RE_Kenshi_log.txt`.

## Arquitectura

- **Plugin** (`src/plugin/`): DLL que carga RE_Kenshi (`startPlugin()`).
  - Hookea el tick del hilo principal (`GameWorld::mainLoop_GPUSensitiveStuff`) vía KenshiLib.
  - Toda mutación del juego ocurre en ese hilo, con cada llamada al motor envuelta en SEH (`__try/__except`).
  - `game/`: fachada sobre el motor (Engine*). `core/`: config, logs, crash dumps y la cola de entrada del hilo de red.
- **Red** (`src/plugin/net/`): ENet sobre UDP en un hilo propio, con túnel opcional por Steam P2P.
  Los datos entrantes pasan al hilo principal por una cola; nunca se toca el motor desde el hilo de red.
- **netproto** (`src/netproto/Wire.h`): protocolo de red en C++03 con structs empaquetados little-endian.
  - `PROTOCOL_VERSION` se comprueba en el handshake: súbelo al cambiar cualquier paquete.
  - `src/prototest` fija el tamaño y el round-trip de cada struct.
- **Sync/replicación** (`src/plugin/sync/`): `Replicator*` (authority, spawn, items, channels, publish, drive),
  interpolación (`Interp`), `ChangeGate`, transferencia de saves (`SaveXfer`).
  Las entidades se identifican con `hand`, que es estable entre saves y entre máquinas.
- **Autoridad:** el host es autoritativo del mundo (NPCs, facciones, tiempo, dinero, edificios);
  cada cliente lo es de **su propio squad**. Las acciones sobre cosas que no posees viajan como
  *intents* (p. ej. `PKT_INV_XFER`) al dueño.
- **Escenarios** (`src/plugin/test/`): solo en las configuraciones Harness y Debug (`KENSHICOOP_HARNESS`).

## Reglas al tocar código

- C++03 / VS2010: sin `auto`, lambdas, `std::mutex` ni `nullptr`; usar `CRITICAL_SECTION`.
- Los frames SEH no pueden contener objetos C++ que necesiten unwinding.
- `GetRealAddress` no sirve para funciones virtuales: usar el gemelo `_NV_<name>`.
- Leer `docs/REPLICATION_PITFALLS.md` antes de cambiar replicación o gates de test.
- `docs/API_REFERENCE.md` §10–13 está desactualizado: menciona `Protocol.h`, `NetClient.h` y
  `MainThreadQueue.h`, que ya no existen. El código real está en `netproto/Wire.h`, `net/NetLink.*` y `core/Inbound.h`.
