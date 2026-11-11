# Changelog

Changes made after the first public release are listed here.

## 0.3.0 (2026-11-11)

### Added
- **Dedicated server admins can change the server's settings from their own Mods menu.** While a server admin is connected, their Easy DD Config menu shows the server's values, and any setting they change is sent to the server, which checks that they really are an admin, applies it at once and saves it in its config file. Only the changed setting is sent. The admin's own settings come back when they leave (or the next time the game starts, if it closed while connected). Server owners can turn this off with `AllowAdminMenuChanges: false` in the server's `EasyDDConfig.cfg`.
- **Allow extreme multipliers (RISKY)**, off by default. Raises the multipliers' limits from x20 (upload speed) and x100 (stacks) to x10,000 and x100,000. The depot still can't hold more of one item than the game can count; the stack multiplier's tooltip shows that limit while the setting is on. Back up your saves before using it.

### Fixed
- Uploaders now reach the configured upload speed even when items arrive faster than one per game tick (faster modded belts, low frame rates, servers with a low tick rate, or a full uploader emptying). Before, an uploader uploaded at most one item per tick. The console variable `EasyDD.UploadTopUp 0` restores the old behaviour, and `EasyDD.TopUpDiag` writes the next few top-up decisions to the game log.
- The depot's displayed upload rate is now exact, e.g. "4,800 per min" instead of "4,799.999" at x20 with all research.
- The plugin's `Version` number now matches the major version (0), which the Satisfactory Mod Repository requires.

### Changed
- README: the mod is now on the Satisfactory Mod Repository, so it can be installed with Satisfactory Mod Manager.

The host or server and every player need 0.3.0; it can't join with 0.2.x.

## 0.2.1 (2026-11-09)

### Changed
- The mod's description on the Mods page now mentions the network batching.
- README: test status updated (listen-server host and the Mod Options switch are now tested).

Because the version number changed, the host or server and every player need 0.2.1; 0.2.0 and 0.2.1 can't join each other.

## 0.2.0 (2026-11-09)

### Added
- **Batch depot network updates (multiplayer)**, a new setting, on by default. On a host or dedicated server, the Dimensional Depot's item-count messages are merged and sent about once a second, and held back while a player's game is behind, so busy depots no longer disconnect players with "Outgoing reliable buffer overflow". Depot counts on screen update about once a second instead of instantly. It follows the approach of the Depot Network Fix mod, with its author's permission, so that mod is no longer needed alongside this one; if it is installed, this mod leaves the job to it.
- `EasyDD.Set` accepts `true` / `false` for the new setting, `EasyDD.Reload` reads it from the config file, and `EasyDD.Status` shows what the batching has done.
