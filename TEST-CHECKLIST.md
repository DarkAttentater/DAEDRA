# In-game test checklist: 0.1.0 (diagnostic build)

This build has **not been run in Skyrim**. It only collects data and loads rules. Please test and send back the log.

## Install

1. Copy `dist\DAEDRA-0.1.0\SKSE` into `Data` (or install the zip through Vortex).
2. Needs SKSE64 and Address Library for SKSE Plugins. SE 1.5.97 and AE 1.6.x are targeted; VR is not.

## Steps

1. Start the game, load a save with several of your problem effects active (a shrine blessing, Beastblood, an enchanted
   circlet, Ordinator perk effects).
2. Confirm the game reaches the main menu and loads the save normally.
3. Open the Magic menu, go to the **Active Effects** tab, wait a second, press **F7**.
4. Close the menu, change to a different load (or reopen the menu), press **F7** again.
5. Edit any file in `SKSE/Plugins/DAEDRA/` (add a deliberate typo), press **F8**, confirm the log reports the error
   with the rule's name, then fix it and press F8 again.
6. Quit the game normally.

## Send back

`Documents\My Games\Skyrim Special Edition\SKSE\DAEDRA.log` (AE with GOG or MS Store may use a different folder name).

## What to look for

* First lines: `DAEDRA 0.1.0 rN starting (runtime ...)` (N must match the zip you installed, `DAEDRA-0.1.0-rN.zip`). If this is missing, the DLL did not load: check
  `Documents\My Games\...\SKSE\skse64.log` for the plugin and the reason.
* `==== Active effects dump ====` blocks listing each effect with `source:` and `caster:` lines.
* `==== MagicMenu Scaleform structure ====`: the tree that decides how rows will be changed.
* Any `[error]` or `[warn]` lines.

## Pass / fail

| Check | Expected |
|---|---|
| Game starts and loads saves | No crash, no extra delay |
| Log written | Yes |
| Dump lists your blessings, Beastblood, circlet effects | Yes, with source names matching the menu's Source column |
| Dump for the circlet: `item (ActiveEffect::source)` line | Tells us whether the worn item is recoverable |
| Rule typo then F8 | Clear error naming file, rule index and id; other rules still load |
| Menu looks and behaves exactly as before | Yes. This build changes nothing in the menu |
