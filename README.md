# Improved Mute Manager

Improved Mute Manager is a C++ plugin for SvenMod that enhances the original mute manager.

To filter muted players IMM uses their Steam ID instead of using "unique" id of player obtained from *cl_enginefunc_t'* function **GetPlayerUniqueID** (it returns player's unique id which is not unique and may collide with id of another player, thus you can mute multiple people when you choose one person to mute). Also you can separately mute voice or chat communications for a specified player.

When you exit the game all muted players will be saved to the file **muted_players.bin** in the root directory of the game, by logical when you load the mod all muted players will be loaded from aforecited file

# How to install

First, download and install SvenMod (see [readme](https://github.com/sw1ft747/SvenMod)), then download the plugins's `.DLL` file and place it in folder `Sven Co-op/svenmod/plugins/`, then add the plugin to file `plugins.txt` (See header `Adding plugins` from SvenMod's [readme](https://github.com/sw1ft747/SvenMod)). 

# Functional

- For filtration, uses Steam ID of a player

- Allows to mute voice and (or) chat communications of a player

- Allows to choose type of muting: mute all communications (voice and chat) or mute separately voice and chat (default)

- Provides console variables and commands to mute players

# Console Variables
ConVar | Default Value | Type | Description
--- | --- | --- | ---
imm_mute_all_communications | 0 | bool | If not zero, mutes all player communications (if you choosed at least voice mute or chat mute), otherwise mutes only separated mute method (voice or chat)
imm_autosave_to_file | 1 | bool | Automatically save all muted players to the file `muted_players.bin`

# Console Commands
ConCommand | Argument #1 | Description
--- | --- | ---
imm_mute_voice | player index | Mute voice chat for a specified player
imm_mute_chat | player index | Mute chat for a specified player
imm_mute_all | player index | Mute voice and chat for a specified player
imm_unmute_voice | player index | Unmute voice chat for a specified player
imm_unmute_chat | player index | Unmute chat for a specified player
imm_unmute_all | player index | Unmute voice and chat for a specified player
imm_unmute_by_steamid64 | Steam64 ID | Unmute all player communications with given Steam64 ID
imm_save_to_file | - | Save all muted players to the file `muted_players.bin`
imm_print_muted_players | - | Print to the console all muted players
imm_print_current_muted_players | - | Print to the console all currently muted players on the server

# How to obtain player index
Type in the console **status** command, it will print detailed information about each player. At the beginning of each line after symbol `#` located player index

How it looks:
```
#index name userid uniqueid connected ping loss state rate adr
#1 "Sw1ft" 2 STEAM_1:0:911469125 00:14 43 0 active 30000 loopback
#2 "Coach" 3 BOT active
#end
```

Player index of Sw1ft is 1.

Player index of Coach is 2.

# Examples
The most easiest just hold TAB and choose a person to mute his voice, mod will guarantee only the chosen person will be muted

Mute player's chat by player's index:
```
imm_mute_chat 3
```

Mute all player communications (chat and voice):
```
imm_mute_all 7
```

Also you can use cvar *imm_mute_all_communications*:
```
imm_mute_all_communications 1
imm_mute_voice 7 // it will mute player's chat too, same will be via TAB
```
