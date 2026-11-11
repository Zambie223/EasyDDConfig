# Easy DD Config

Set the Dimensional Depot's **upload speed** and **stack limit** from the in-game **Mod Options** menu. MAM research upgrades still apply on top of your settings.

## Installation

You need Satisfactory 1.2 (build 517554 or newer) with Satisfactory Mod Loader (SML) 3.12 or newer installed.

**The easy way:** install [Easy DD Config from the Satisfactory Mod Repository](https://ficsit.app/mod/EasyDDConfig) with [Satisfactory Mod Manager](https://github.com/satisfactorymodding/SatisfactoryModManager/releases/latest). It installs SML for you.

**By hand**, from this repository's releases:

1. From the Releases page, download the zip for where it will run:
   - `EasyDDConfig-Windows.zip` for your game
   - `EasyDDConfig-WindowsServer.zip` or `EasyDDConfig-LinuxServer.zip` for a dedicated server
2. Extract it into `FactoryGame/Mods/EasyDDConfig/` inside the game or server folder, so that `EasyDDConfig.uplugin` sits directly in that folder.
3. Start the game or server. The mod appears under **Mods** in the main menu.

In multiplayer, every player and the host or server need the same version.

## Settings

Open **Mods → Easy DD Config** from the main menu or the pause menu. Changes apply immediately, without reloading the world.

| Setting | Default | Range | What it does |
|---|---|---|---|
| Upload speed multiplier | 1.0 | 0.1 – 20 | Multiplies upload speed at every research level. Vanilla is 15 items/min per uploader at base and 240 with all research, so 2.0 gives 30 and 480. It also speeds up your own upload slots, as vanilla research does. |
| Stack limit multiplier | 1.0 | 1 – 100 | Multiplies how many stacks of each item the depot can hold, including research upgrades. Vanilla is 1 stack at base and 5 with all research, so 3.0 gives 3 and 15. It can't go below vanilla, and the total is capped at 500 stacks per item type. |
| Allow extreme multipliers (RISKY) | Off | On / Off | Raises the limits above to ×10,000 (upload speed) and ×100,000 (stacks). See [Extreme multipliers](#extreme-multipliers). |
| Batch depot network updates (multiplayer) | On | On / Off | Only matters when you host or run a dedicated server. See [Multiplayer](#multiplayer). |

A multiplier of 1.0 behaves exactly like the base game. Multipliers are rounded to one decimal place when applied, so 2.37 is used as 2.4.

### Extreme multipliers

**Back up your saves before turning this on.** With **Allow extreme multipliers** on, the upload speed multiplier goes up to ×10,000 (2,400,000 items/min per uploader with all research) and the stack limit multiplier up to ×100,000 (500,000 stacks per item with all research). The game's own limits still apply on top:

- The depot can't hold more of one item than the game can count (2,147,483,647). Stack totals are cut so that the largest stack size of any item can't go past that; the stack multiplier's tooltip shows the limit while the setting is on. In the base game the ×100,000 limit is reached first.
- Uploads are limited by what is in each uploader, the room left in the depot, and how much your PC or server can process. Very high rates cost CPU, and network traffic in multiplayer, so the game may slow down or stutter, and real upload rates can end up below the configured ones.

Turning the setting off clamps the multipliers back to the normal limits; nothing in the depot is deleted. Close and reopen the Mods menu after changing it, so the tooltips and limits shown are up to date.

## Multiplayer

- **Install the mod on the server or host and on every player's game.** Players without it can't join.
- **The host's settings are used, or the server's on a dedicated server.** Changing Mod Options as a joined player does nothing while you are connected. Your own settings come back when you play your own world.
- **Dedicated server admins can change the server's settings from their own Mods menu.** While you are connected as a server admin, your Easy DD Config menu shows the server's values, and any setting you change there is sent to the server. The server checks that you really are an admin, applies the change at once, saves it in its config file, and confirms it in chat. Only the setting you changed is sent. Your own settings come back when you leave the server (or the next time the game starts, if it closed while you were connected). A server owner can turn this off with `"AllowAdminMenuChanges": false` in the server's config file.
- **Depot network messages are batched, so busy depots don't disconnect players.** The game sends a network message to every player each time a depot item count changes, and disconnects a player whose game falls 511 messages behind ("Outgoing reliable buffer overflow"), for example while loading in after joining. Faster uploads and bigger stack limits mean many more of these messages: without batching, a factory with 660 uploaders went from 0 to about 2,400 per second, peaking above 5,000. With **Batch depot network updates** on (the default), the host or server keeps the latest count of each item and sends the changed ones together about once a second, and waits while a player's game is behind. Depot counts on screen then update about once a second instead of instantly; what's actually in the depot is unaffected. The setting does nothing in single player.
  - This works like the [Depot Network Fix](https://ficsit.app/mod/DepotNetworkFix) mod, whose approach it follows with its author's permission, so you don't need both. If Depot Network Fix is installed too, this mod leaves the job to it.

### Dedicated servers

Dedicated servers have no menu of their own. Use any of these:

- **Join as a server admin** and use your own Mods menu (see above).
- **Edit the config file.** It is `FactoryGame/Configs/EasyDDConfig.cfg` in the server's install folder, and it is created on first start:
  ```json
  {
      "UploadSpeedMultiplier": 2,
      "StackLimitMultiplier": 3,
      "AllowExtremeMultipliers": false,
      "BatchDepotNetworkUpdates": true,
      "AllowAdminMenuChanges": true,
      "SML_ModVersion_DoNotChange": "0.3.0"
  }
  ```
  Then restart the server, or run `EasyDD.Reload` (below). `AllowAdminMenuChanges` exists only in the config file; it isn't shown in the Mods menu.
- **Use the console.** Run these commands from the Server Manager console, which needs admin rights:

  | Command | Effect |
  |---|---|
  | `EasyDD.Status` | Shows the vanilla, configured and effective values, what network batching and upload top-ups have done, and whether admin menu changes are allowed. |
  | `EasyDD.Set UploadSpeedMultiplier 2` | Changes and saves a setting. It applies within a second. Switches take `true` or `false`, e.g. `EasyDD.Set BatchDepotNetworkUpdates false` or `EasyDD.Set AllowAdminMenuChanges false`. |
  | `EasyDD.Reload` | Re-reads `EasyDDConfig.cfg` after you edit it. |
  | `EasyDD.UploadTopUp 0` | Turns upload top-ups off (see [Limits and notes](#limits-and-notes)); `1` turns them back on. Not saved: it's back on after a restart. |
  | `EasyDD.TopUpDiag` | Writes the next few upload top-up decisions to the game log (`FactoryGame.log`). Useful when reporting a problem with upload speeds. |

## Limits and notes

- **Upload top-ups:** the base game uploads at most one item per uploader per game tick (60 per second at 60 frames per second; a dedicated server ticks 30 times a second by default). An uploader holds one stack and is filled by its belt, so this only matters when items arrive faster than the game ticks: with faster modded belts, on a server running at a low tick rate, or when a full uploader empties after the depot gets room again. Since 0.3.0, when an uploader is due more than one item in a tick, the mod uploads the rest through the game's own upload function, so the configured speed is reached in those cases too. It only acts when an item takes less than one tick to upload. `EasyDD.UploadTopUp 0` in the console turns it off. For reference, a Mk.6 belt carries 1,200 items/min (20 per second), which is ×5 at full research.
- **Displayed upload rate:** the game's own rounding showed rates a hair low (e.g. "4,799.999 per min" at ×20 with all research). Since 0.3.0 the mod corrects the value the game displays, so every setting up to ×20 shows its exact rate. With extreme multipliers above about 8,000 items/min a tiny fraction can still show, because the game stores the time as a single-precision number.
- **Other mods:** don't combine this mod with other mods that change the same depot settings. If something else overwrites them, this mod puts its values back within a few seconds and logs a warning.
- **Saves:** nothing is written into your save. Removing the mod returns the depot to vanilla values.
- **Lowering the limit or removing the mod never deletes items.** If the depot holds more of an item than the new limit allows, everything stays and you can still take it out. Uploads of that item pause until the amount drops below the limit.

## Status

Version 0.1.0 has been tested on Windows in single player, as a listen-server host with a joined player, and on Windows and Linux dedicated servers with a joined player, using large factories with up to 660 uploaders:
- the Mod Options menu (main menu and pause menu), live changes, the config file and console commands
- research upgrades and save/load
- host values reaching a connected player, and a joined player's own settings being ignored
- upload speed and its limit, lowering limits, and removing the mod
- network load with a connected player

Version 0.2.0 added network batching, tested with ×20 upload speed and ×20 stack limit:
- **Windows dedicated server, a factory with 660 uploaders, one joined player:** about 13,000 depot count updates a minute were sent as about 1,700 messages. While the player was loading in, sends waited until its queue (peaking at 280 of the 511 limit) went down, and the player stayed connected.
- **Listen-server host, a factory with 16 uploaders, one joined player:** about 3,000 updates a minute were sent as about 550 messages, and the player stayed connected.
- Turning the setting off and on took effect immediately from the Mod Options menu, the console and the config file.
- When a mod named Depot Network Fix is installed, this mod detects it and leaves the job to it. That detection was checked with a placeholder; the two mods have not been run together.
- The mod also loads and hooks correctly on a Linux dedicated server.

Version 0.3.0 was tested in single player and on Windows and Linux dedicated servers, with factories of 16 and 660 uploaders:
- **Admin menu sync (Windows dedicated server, one joined admin):** the admin's Mods menu showed the server's values; a changed setting was applied and saved by the server, and only that setting was sent; the admin's own values came back after leaving. Turning `AllowAdminMenuChanges` off and on was checked from the console.
- **Upload top-ups:** in single player at 20 frames per second, a full uploader drained at the configured ×20 rate (80 items a second, about 4 per frame instead of the game's 1). On a dedicated server at ×20 with 660 uploaders, about 1,000 extra items a second were uploaded, and the server's CPU use was the same with top-ups on and off.
- **Extreme multipliers:** at ×10,000 upload and ×100,000 stacks, the server stayed responsive (slowest API reply 0.12 s); the stack total was capped at the technical limit; turning the setting off clamped both multipliers back. The checkbox and tooltips were checked in the Mods menu.
- **Displayed upload rate:** an uploader showed 4,800 per minute at ×20 with all research.
- Both hooks also load on a Linux dedicated server.

It has not been tested in an internet game across several PCs; all multiplayer tests ran on one PC. The network-load figures above may be worse over real connections.

## Building from source

1. Set up the [SML starter project](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/index.html) with the Unreal Engine version it requires.
2. Clone this repository into the starter project as `Mods/EasyDDConfig`.
3. Package it with Alpakit. Building the LinuxServer target also needs Unreal's Linux cross-compile toolchain.

`Tools/create_config_asset.py` creates or updates the Mod Options asset (`Content/EasyDDConfiguration.uasset`) and `Tools/verify_config_asset.py` checks it. Both run headless in the editor, with instructions at the top of each file. `Tools/make_icon.py` redraws the icon and needs Python with Pillow.

## License

Copyright (C) 2026 Zambie223

Easy DD Config is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE](LICENSE) for the full text.

## Responsible AI use disclosure

Most of the code, tooling and documentation in this mod were written by an AI coding assistant (Claude, made by Anthropic, used through Claude Code), working under the maintainer's direction. The maintainer set the requirements and design decisions. Correctness was checked with hands-on runs against a real Satisfactory dedicated server and game client (see Status above), but not every line has been reviewed by a person. AI-written code can contain mistakes: review it before relying on it, and back up your saves.
