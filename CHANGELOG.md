# Changelog

Changes made after the first public release are listed here.

## Unreleased

### Changed
- New icon: a glowing settings gear inside a wireframe cube. The Mod Options menu shows it from the next release. Its source is now `Branding/Icon.svg`, and `Tools/make_icon.py` renders it to the PNG icons.

## 0.3.2 (2026-11-21)

Needs game version 1.2.5.1 (build 520140) or newer and SML 3.12.1 or newer. On older game versions, keep 0.3.1.

### Fixed
- **Multiplayer on game 1.2.5.1:** a host or dedicated server no longer crashes once a player is connected while **Batch depot network updates** is on. The mod is now built for 1.2.5.1, so it reads the game's network connection data correctly again. This fixes the 0.3.1 known issue: if you turned batching off because of it, turn it back on. A test over a simulated slow connection showed it is still needed on 1.2.5.1: with it off, a player joining a busy factory was disconnected while loading in.

### Added
- A startup check for game updates. If an update changes data this mod reads directly, the affected part (the depot settings, network batching, admin menu sync or chat messages) stays off and the game log says which, instead of the game crashing.

### Changed
- The mod's startup log line no longer shows how many uploaders the depot has registered. That number was always 0 at that point; the count of uploaders in the world is still shown.

The host or server and every player need the same version.

## 0.3.1 (2026-11-19)

### Fixed
- Upload top-ups work again on game version 1.2.5.1. The patch moved where the game keeps an uploader's inventory, so the mod no longer recognised uploaders and top-ups silently stopped (no crash; uploads fell back to one item per uploader per frame). The mod now looks the inventory up through the game's reflection data, which follows such moves.

### Known issue
- **Multiplayer on game 1.2.5.1:** the patch also changed the game's network connection data, which **Batch depot network updates** reads. On 1.2.5.1 this can crash a host or dedicated server once a player is connected. Until a multiplayer update, hosts and servers on 1.2.5.1 should turn **Batch depot network updates** off (Mod Options, or `EasyDD.Set BatchDepotNetworkUpdates false` on a server). Single player is not affected.

The host or server and every player need the same version.

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
