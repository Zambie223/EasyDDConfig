# Easy DD Config

Set the Dimensional Depot's **upload speed** and **stack limit** from the in-game **Mod Options** menu. MAM research upgrades still apply on top of your settings.

## Installation

You need Satisfactory 1.2 (build 517554 or newer) with Satisfactory Mod Loader (SML) 3.12 or newer installed.

The mod isn't on the Satisfactory Mod Repository yet, so Satisfactory Mod Manager can't install it. Install a release from this repository by hand:

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

A value of 1.0 behaves exactly like the base game. Values are rounded to one decimal place when applied, so 2.37 is used as 2.4.

## Multiplayer

- **Install the mod on the server or host and on every player's game.** Players without it can't join.
- **The host's settings are used, or the server's on a dedicated server.** Changing Mod Options as a joined player does nothing while you are connected. Your own settings come back when you play your own world.
- **Bigger stack limits mean more network traffic.** The game sends a network message to every player for each item uploaded. In vanilla, a large factory's depot is often full, so uploads stop and that traffic stops with them. A higher stack limit keeps every uploader running. In testing, a factory with 660 uploaders went from 0 to about 2,400 messages per second, peaking above 5,000 per second while queued items drained at high upload speed. A player on the same PC stayed connected through it, but internet connections have more delay and can overload sooner, which disconnects players ("Outgoing reliable buffer overflow"). If you use high multipliers in multiplayer, also install [Depot Network Fix](https://ficsit.app/mod/DepotNetworkFix) on the server.

### Dedicated servers

Dedicated servers have no menu. Use either of these:

- **Edit the config file.** It is `FactoryGame/Configs/EasyDDConfig.cfg` in the server's install folder, and it is created on first start:
  ```json
  {
      "UploadSpeedMultiplier": 2,
      "StackLimitMultiplier": 3,
      "SML_ModVersion_DoNotChange": "0.1.0"
  }
  ```
  Then restart the server, or run `EasyDD.Reload` (below).
- **Use the console.** Run these commands from the Server Manager console, which needs admin rights:

  | Command | Effect |
  |---|---|
  | `EasyDD.Status` | Shows the vanilla, configured and effective values. |
  | `EasyDD.Set UploadSpeedMultiplier 2` | Changes and saves a setting. It applies within a second. |
  | `EasyDD.Reload` | Re-reads `EasyDDConfig.cfg` after you edit it. |

## Limits and notes

- **Upload limit:** one uploader tops out at about 60 items per second (3,600 items/min) when the game runs at 60 frames per second or less, and a dedicated server reached about 3,200 items/min. With all research that's reached at about ×13–15, so higher values usually change nothing there. In single player at higher frame rates the limit rises: ×20 with all research (4,800 items/min) was reached at about 140 frames per second. Without research, ×20 is only 300 items/min, so the full range is still useful. For reference, a Mk.6 belt carries 1,200 items/min, which is ×5 at full research.
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
