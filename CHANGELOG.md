# Changelog

Changes made after the first public release are listed here.

## 0.2.0 (2026-11-09)

### Added
- **Batch depot network updates (multiplayer)**, a new setting, on by default. On a host or dedicated server, the Dimensional Depot's item-count messages are merged and sent about once a second, and held back while a player's game is behind, so busy depots no longer disconnect players with "Outgoing reliable buffer overflow". Depot counts on screen update about once a second instead of instantly. It follows the approach of the Depot Network Fix mod, with its author's permission, so that mod is no longer needed alongside this one; if it is installed, this mod leaves the job to it.
- `EasyDD.Set` accepts `true` / `false` for the new setting, `EasyDD.Reload` reads it from the config file, and `EasyDD.Status` shows what the batching has done.
