# CLAUDE.md — TokelaCoop (fork de dentroytu; hasta la v0.53 se llamaba KenshiCoop)

Mod cooperativo de Kenshi, fork de `nhoral/KenshiCoop` (base: v0.51, commit `5a761e1`).
Remotes: `origin` = dentroytu/TokelaCoop (antes dentroytu/KenshiCoop; GitHub redirige), `upstream` = nhoral/KenshiCoop.

**Nombre y versión.** Desde la v0.54 el proyecto se llama **TokelaCoop**: `TokelaCoop.dll` en
`mods\TokelaCoop`, `TokelaCoop.mod` en `data\mods.cfg`, variables `TOKELACOOP_*`. La versión se escribe
en un solo sitio, `src/netproto/Version.h` (`TOKELACOOP_VERSION`); el juego la muestra en el banner de arriba
a la izquierda y en el título del panel F2 ("TokelaCoop v0.54"), y el kit la estampa en `PROVENANCE.json` y en
la descripción del `.mod`. No hay que cambiar nunca los StringIds `N-KenshiCoop-MultiplayerStart.mod` del
`.mod`: las partidas guardadas los usan. Si un Kenshi tiene cargada a la vez la DLL antigua `KenshiCoop.dll`,
TokelaCoop no se activa y avisa.

> **Estado (2026-09-25):**
> - Compilación verificada en CI (`windows-2022`): `prototest`, `tunneltest`, DLL Harness y Release, y kit con instalador.
>   Releases publicadas: `v0.52` y `v0.53` (con el nombre KenshiCoop).
> - **Sin probar en el juego (v0.54):** cargar como `TokelaCoop.dll` desde `mods\TokelaCoop`, la migración desde
>   `mods\KenshiCoop` (instalador y `deploy.cmd`), el banner/F2/descripción con "TokelaCoop v0.54", los inicios
>   renombrados y las partidas de prueba (`fixtures/saves`), que siguen listando el mod "KenshiCoop".
> - **Verificado en el juego** (PC del autor del fork, Kenshi de Steam con RE_Kenshi):
>   - el plugin carga;
>   - el panel F2 abre en el menú principal, detecta el idioma (español) y colorea el estado (v0.53);
>   - el botón de invitar aparece y lista amigos (v0.52);
>   - el instalador del kit funcionó con un RE_Kenshi ya instalado;
>   - co-op por UDP con dos instancias en un PC: `coop_presence` PASS (2026-09-24);
>   - cerrar Kenshi con el panel F2 abierto o con la ventana enfocada ya no crashea (2026-09-24);
>   - rechazos del host (versión distinta, sesión llena) contra un Kenshi real con `kcprobe` (2026-09-25).
> - **Sin probar en el juego:**
>   - sesión co-op con un amigo por Steam;
>   - la comprobación de mods;
>   - la instalación de RE_Kenshi desde cero con el instalador;
>   - el flujo completo de invitación, que necesita otra cuenta de Steam.

## Próximos pasos (al retomar, p. ej. desde Windows)

1. **Co-op real en un solo PC por UDP** (hecho el 2026-09-24): `scripts\setup_join_install.cmd` (desde PowerShell) crea
   `%USERPROFILE%\Kenshi-Join`, `scripts\deploy.cmd` despliega en las dos y `scripts\dev_cycle.ps1 -SkipBuild` lo prueba.
   `dev_cycle.ps1` cierra a la fuerza cualquier Kenshi abierto: avisa antes de lanzarlo.
2. **Toolchain local en Windows** (hecho el 2026-09-24 en el PC del autor), en una PowerShell de **administrador** y de uno en uno (dos instalaciones MSI a la vez fallan con 1618):
   - VS2022 Build Tools: `winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"`;
   - `scripts\setup_toolchain.ps1`: los mismos pasos que `.github/workflows/build.yml` (MSIs del SDK 7.1 sacados de la ISO, KB2519277 y la clave de registro VS7).
     Desinstala el redistributable de VC++ 2010 para que el SDK se instale y lo vuelve a instalar al final:
     sin él, el launcher de Kenshi falla con "no se encontró mfc100u.dll";
   - `scripts\fetch_deps.ps1` (no necesita administrador);
   - `scripts\build_plugin.cmd`.
3. **Con el amigo (2 cuentas de Steam):** invitación completa desde "Invitar a un amigo de Steam", comprobación de mods y sesión larga.
4. **Instalador:** instalación de RE_Kenshi desde cero y un Kenshi que no esté en `C:`.
5. **Hoja de ruta** (`docs/MODS_Y_FACILIDAD.md` §5):
   - error de versión visible, con el motivo en la desconexión, sin reintentos infinitos y mostrado en F2;
   - "Reconectar con <amigo>";
   - unirse desde la lista de amigos de Steam;
   - aviso de versión nueva;
   - Steam Workshop.

Flujo de trabajo:
- una rama por cambio, PR y fusión cuando el CI está en verde;
- las releases se publican con una etiqueta `vX.YY` (última: `v0.53`), que tiene que coincidir con
  `TOKELACOOP_VERSION` de `src/netproto/Version.h` (el CI rechaza la etiqueta si no);
- el dueño prefiere español y una UX muy sencilla.

## Objetivo del fork

**Co-op de 2 jugadores (host + 1 amigo), muy fácil de instalar y de conectar.**
El soporte para 3–4 jugadores queda aparcado (2026-09-23). Si se retoma, hay referencias en
`LogoutUser/KenshiCoopTrio` (relay en el host, ids por jugador, túnel Steam multi-peer).
Ojo: está 9 versiones de protocolo por detrás y sus ids de paquete chocan con los nuestros.

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

```powershell
powershell -ExecutionPolicy Bypass -File scripts\fetch_deps.ps1   # necesita git + git-lfs; idempotente
```

Qué hace (y por qué):
- `KenshiLib_Examples_deps` @ `b566d74` (KenshiLib 0.4.0, Git LFS): `KenshiLib.lib`, Ogre/MyGUI, Boost 1.60.
  Su `KenshiLib.lib` exporta los 185 símbolos que importa la DLL v0.51 publicada.
- Sustituye las cabeceras por las del repo fuente `BFrizzleFoShizzle/KenshiLib` @ `b0d7665` (2026-08-09).
  Las cabeceras empaquetadas no compilan juntas: `BuildingDesignation` está definido dos veces y `CraftingItem` está incompleto.
  El autor tenía cabeceras parcheadas a mano en local (ver `src/plugin/game/ZoneQuery.cpp:1-5`).
- Añade un `kenshi/CombatClass.h` que reenvía a `kenshi/combat/CombatClass.h`: la clase se movió de carpeta
  y los offsets no cambian.
- ENet v1.3.18 + los dos parches de `third_party/enet/patches/`.

- Los parches se aplican desde la raíz del repo. El 0001 hace ENet 1.3.18 compatible con C89 (v100).
- `third_party/vc10_compat/` es un shim versionado (`ammintrin.h`, `OgreConfig.h`, `OgrePlatformInformation.h`).
- Las fuentes de ENet se compilan directamente dentro de `TokelaCoop.vcxproj`.

## CI (compilar sin Windows)

`.github/workflows/build.yml` compila la DLL (Harness y Release) y ejecuta `prototest` en `windows-2022`
en cada push. Instala el toolchain v100 igual que `tools/*.ps1`. Los artefactos son las DLLs.
Si solo tienes el Mac, haz push y descarga la DLL del run:
`gh run download --repo dentroytu/TokelaCoop -n TokelaCoop-<sha>`.

## Compilar

```bat
scripts\build_plugin.cmd            :: Harness (por defecto): DLL de test con scenario runner
scripts\build_plugin.cmd Release    :: DLL para jugadores (sin código de escenarios)
scripts\build_plugin.cmd Debug
scripts\build_prototest.cmd         :: dist\prototest.exe: tests unitarios del wire protocol
```

Salida: `src\plugin\x64\<Config>\TokelaCoop.dll`.

## Desplegar

```bat
scripts\deploy.cmd ["C:\ruta\a\Kenshi"] [Harness|Release|Debug]
```

Copia la DLL, `RE_Kenshi.json` y `TokelaCoop.mod` a `<Kenshi>\mods\TokelaCoop\`, en la instalación de Steam y en
`Kenshi-Join`. Después `scripts\migrate_install.ps1` deja solo TokelaCoop activo en cada una: lleva el
`coop_config.json` de `mods\KenshiCoop`, cambia en `data\mods.cfg` la línea `KenshiCoop.mod` por `TokelaCoop.mod`
en su sitio (o la añade) y borra `mods\KenshiCoop`.
Por defecto usa la ruta de Steam. Requisitos en el juego: Kenshi 1.0.65 (Steam) + RE_Kenshi 0.3.1+.

Compatibilidad comprobada con análisis estático (2026-09-23), no ejecutando el juego:
- Los 185 símbolos que la DLL v0.51 importa de `KenshiLib.dll` los exportan RE_Kenshi 0.3.4 y 0.3.5.
- RE_Kenshi 0.3.5 admite Kenshi Steam/GOG 1.0.65 y 1.0.68 (`config.json`).
- La `KenshiLib.lib` de deps `b566d74` exporta los mismos 9789 símbolos que la `KenshiLib.dll` de RE_Kenshi 0.3.4.
- Tras cambiar cabeceras o KenshiLib, repetir la comparación con la DLL nueva: importaciones de la DLL
  (`llvm-objdump -p`) frente a exportaciones de la `KenshiLib.dll` de RE_Kenshi.

## Kit de jugador, instalador y releases

- Fuentes del kit en `kit/`:
  - `README.txt`;
  - `Instalar TokelaCoop.cmd`;
  - `installer/Install-TokelaCoop.ps1`: el flujo interactivo;
  - `installer/TokelaCoopInstaller.psm1`: la lógica, testeable.
- `scripts/make_mod_kit.ps1 [-SkipBuild]` arma `dist/mod-kit/` y `dist/TokelaCoop-kit.zip`.
- El instalador hace esto:
  - busca Kenshi en las librerías de Steam (`libraryfolders.vdf`), en GOG o preguntando;
  - si falta RE_Kenshi, descarga la 0.3.5 de GitHub, verifica su SHA-256 y abre su instalador oficial
    (obligatorio en 1.0.68; no tiene modo silencioso);
  - copia a `mods\TokelaCoop` sin pisar `coop_config.json` (si no hay, trae el de `mods\KenshiCoop`);
  - añade `TokelaCoop.mod` a `data\mods.cfg`, sustituyendo en su sitio una línea `KenshiCoop.mod`;
  - borra `mods\KenshiCoop` (después de quitarlo de `mods.cfg`, para que nunca estén los dos activos);
  - avisa si hay una copia en Workshop con cualquiera de los dos nombres.
- Scripts en PowerShell 5.1, con los `.ps1`/`.psm1` en UTF-8 con BOM (si no, 5.1 rompe las tildes).
- Tests sin juego, que el CI ejecuta con `powershell` 5.1:
  - `scripts/tests/Installer.Tests.ps1`, con carpetas falsas (incluye la migración y el `.mod` publicado);
  - `scripts/tests/Contract.Tests.ps1` (manifiesto, oráculos y que no quede ningún `KENSHICOOP_`).
- El `.mod` (`dist/mods/TokelaCoop/TokelaCoop.mod`) lo genera `tools/MultiplayerStartGen` (necesita .NET 9 y un
  Kenshi); para cambiar solo textos (nombres, descripciones) basta `scripts/ModText.psm1`, que recalcula la
  longitud de cada registro y se niega a tocar un StringId.
- CI: cada push sube el artefacto `TokelaCoop-kit-<sha>`.
- Release: `scripts\set_version.ps1 -Version X.YY` (cambia `Version.h`, la descripción del `.mod` del repo, el
  generador y el README a la vez), commit, y luego `git tag vX.YY && git push origin vX.YY` publica la Release con
  el zip y su `.sha256` (el CI falla si la etiqueta no es `v` + esa versión).
- Cambiar de versión de RE_Kenshi requiere tocar dos sitios de `TokelaCoopInstaller.psm1`:
  `$REKenshiRelease` (URL + SHA-256) y `$KnownKenshiLib` (hash de su `KenshiLib.dll`).

## Testear (harness de dos clientes en una sola máquina)

- **Host:** la instalación de Steam, `C:\Program Files (x86)\Steam\steamapps\common\Kenshi`
  (fija en `dev_cycle.ps1` y en `deploy.cmd` por defecto).
- **Join:** una copia independiente en `%USERPROFILE%\Kenshi-Join`, creada con
  `scripts\setup_join_install.cmd`. Es seguro repetirlo: conserva los `save/` y la configuración del join.
- Los clientes se configuran por variables de entorno `TOKELACOOP_*` (MODE, IP, PORT,
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
- Handshake sin segundo Kenshi: `scripts\build_kcprobe.cmd` y `dist\kcprobe.exe client --version N [--hold MS] [--exit goodbye|crash] [--junk] [--retry MS]`,
  un cliente falso contra un host real (UDP). Solo para pruebas; no va en el kit.
- `scripts\build_tunneltest.cmd` y `dist\tunneltest.exe`: ENet por el modelo del túnel de Steam (1200 bytes, pérdida), sin juego. Lo ejecuta el CI.
- En el juego: `<Kenshi>\TokelaCoop_host.log` / `_join.log` y `RE_Kenshi_log.txt`.

## Arquitectura

- **Plugin** (`src/plugin/`): DLL que carga RE_Kenshi (`startPlugin()`).
  - Hookea el tick del hilo principal (`GameWorld::mainLoop_GPUSensitiveStuff`) vía KenshiLib.
  - Toda mutación del juego ocurre en ese hilo, con cada llamada al motor envuelta en SEH (`__try/__except`).
  - `game/`: fachada sobre el motor (Engine*). `core/`: config, logs, crash dumps y la cola de entrada del hilo de red.
- **Red** (`src/plugin/net/`): ENet sobre UDP en un hilo propio, con túnel opcional por Steam P2P.
  Los datos entrantes pasan al hilo principal por una cola; nunca se toca el motor desde el hilo de red.
- **netproto** (`src/netproto/Wire.h`): protocolo de red en C++03 con structs empaquetados little-endian.
  - `PROTOCOL_VERSION` se comprueba en el handshake: súbelo al cambiar cualquier paquete.
  - Rechazos: el host desconecta con un código en el u32 de DISCONNECT de ENet (`refuseEncode`: VERSION, FULL con reintento).
    Regla: el host **nunca** desconecta con 0. Así rechazan las versiones hasta la v0.53, y un cliente tiene que poder distinguirlas.
  - Sesión de 2: el host rechaza como FULL a un segundo peer mientras hay uno admitido, y el cliente se despide al parar
    (también al cerrar Kenshi) para liberar la plaza al momento.
  - `src/prototest` fija el tamaño y el round-trip de cada struct.
- **Sync/replicación** (`src/plugin/sync/`): `Replicator*` (authority, spawn, items, channels, publish, drive),
  interpolación (`Interp`), `ChangeGate`, transferencia de saves (`SaveXfer`).
  Las entidades se identifican con `hand`, que es estable entre saves y entre máquinas.
- **Autoridad:** el host es autoritativo del mundo (NPCs, facciones, tiempo, dinero, edificios);
  cada cliente lo es de **su propio squad**. Las acciones sobre cosas que no posees viajan como
  *intents* (p. ej. `PKT_INV_XFER`) al dueño.
- **Escenarios** (`src/plugin/test/`): solo en las configuraciones Harness y Debug (`TOKELACOOP_HARNESS`).

## Reglas al tocar código

- C++03 / VS2010: sin `auto`, lambdas, `std::mutex` ni `nullptr`; usar `CRITICAL_SECTION`.
- Los frames SEH no pueden contener objetos C++ que necesiten unwinding.
- `GetRealAddress` no sirve para funciones virtuales: usar el gemelo `_NV_<name>`.
- Leer `docs/REPLICATION_PITFALLS.md` antes de cambiar replicación o gates de test.
- `docs/API_REFERENCE.md` §10–13 está desactualizado: menciona `Protocol.h`, `NetClient.h` y
  `MainThreadQueue.h`, que ya no existen. El código real está en `netproto/Wire.h`, `net/NetLink.*` y `core/Inbound.h`.
