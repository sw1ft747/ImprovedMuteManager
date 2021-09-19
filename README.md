# Advanced Mute System

This is injectable module **(with good intentions)** considered as a mod for Sven Co-op

To filter muted players AMS uses their Steam ID instead of using "unique" id of player obtained from *cl_enginefunc_t'* function **GetPlayerUniqueID** (it returns player's unique id which is not unique and may collide with id of another player, thus you can mute multiple people when you choose one person to mute)

When you exit the game all muted players will be saved to the file **muted_players.db** in the root directory, by logical when you load the mod all muted players will be loaded from aforecited file

# Auto Injection

Place files `advanced_mute_system.dll`, `mm_injector.exe` and `mm_injector.ini` in the root directory of the game, create a shortcut to the `mm_injector.exe` file and place it in anywhere you like, also you can add it as label in Steam. Now you can launch it to auto-inject the mod (see `mm_injector.ini` to change settings)

When the mod successfully started you can see in the console corresponding message

### VAC

There's no VAC in the game, but anyway for safe you can use `-insecure` launch parameter, you will be able to join any secure VAC server

# Functional

- Uses for filtration Steam ID of a player

- Uses hash table for fast access to a muted player

- Allows to mute voice and (or) chat communications of a player

- Allows to choose type of muting: mute everything (voice and chat) or mute separately voice and chat (default)

- Provides console commands to mute players

# Console Variables
ConVar | Default Value | Type | Description
--- | --- | --- | ---
ams_mute_everything | 0 | bool | If not zero, mutes all player communications (if you choosed at least voice mute or chat mute), otherwise mutes only separated mute method (voice or chat)

# Console Commands
ConCommand | Argument #1 | Description
--- | --- | ---
ams_mute_voice | player index | Mute voice chat for a specified player
ams_mute_chat | player index | Mute chat for a specified player
ams_mute_all | player index | Mute voice and chat for a specified player
ams_unmute_voice | player index | Unmute voice chat for a specified player
ams_unmute_chat | player index | Unmute chat for a specified player
ams_unmute_all | player index | Unmute voice and chat for a specified player
ams_show_muted_players | - | Print to the console all muted players
ams_show_current_muted_players | - | Print to the console all muted players on the current server

# Autoexec
When mod was injected, file **ams_autoexec.cfg** will be automatically executed from folder *Sven Co-op/svencoop/*, you can use it to save value of console variable **ams_mute_everything**

# How to get player index
Type in the console **status** command, it will print detailed information about each player. At the beginning of each line after symbol `#` located player index

How it looks:
```
#index name userid uniqueid connected ping loss state rate adr
#1 "Sw1ft" 2 STEAM_1:0:911469125 00:14 43 0 active 30000 loopback
#2 "Coach" 3 BOT active
#end
```

# Examples
The most easiest just hold TAB and choose a person to mute his voice, mod will guarantee only the chosen person will be muted

Mute player's chat by index:
```
ams_mute_chat 3
```

Mute all player communications:
```
ams_mute_all 7
```

Also you can use cvar *ams_mute_everything*:
```
mute_everything 1
mute_voice 7 // it will mute chat too, same will be via TAB
```
