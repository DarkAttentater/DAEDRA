# M0: Active Effects list, research status

Status: **partly done from headers only; the in-game half is waiting on a test run.**
Nothing here has been verified in a running game.

## What CommonLibSSE-NG 3.7.0 tells us (read from source)

| Question | Finding | Confidence |
|---|---|---|
| How are active effects stored? | `MagicTarget::GetActiveEffectList()` returns `BSSimpleList<ActiveEffect*>*`. The player reaches it through `Actor::AsMagicTarget()`. | High, it is a vtable entry in CommonLib |
| What does an `ActiveEffect` know? | `spell` (`MagicItem*`: SpellItem, EnchantmentItem, AlchemyItem, IngredientItem, ScrollItem), `effect` (`Effect*`, holds `baseEffect` = the MGEF), `source` (`TESBoundObject*`), `caster` (handle), `displacementSpell`, `elapsedSeconds`, `duration`, `magnitude`, `flags`, `castingSource`. | High |
| Is `source` the worn item for enchantments? | Unknown. Field name suggests the originating object; the diagnostic dump prints it for every effect so we can see. | **Hypothesis** |
| Is the menu vanilla or SkyUI? | `RE::MagicMenu` exists in CommonLib with a `GFxValue root` ("Menu_mc"). SkyUI replaces the SWF but the menu class is the same. | Medium |
| Which function fills each row? | Not determinable from headers. | **Open** |
| Where do row fields (Name / Time Left / Source) come from? | Not determinable from headers. | **Open** |

## What the diagnostic build does to close the gaps

With `dumpActiveEffects = true` (default in the shipped INI) the plugin writes, each time the Magic menu opens, one block per
active effect: MGEF form/keywords/archetype/actor value, the source spell with kind and spell type, the caster, the
`ActiveEffect::source` item, and the rule that would match.

Pressing the dump key (F7 by default) with the Magic menu open on the Active Effects tab also logs the Scaleform tree of
`_root` (depth 5, first three rows of every array). That shows:

1. whether the rows are an array on some `_root...` path and what fields each row object has (name, time left, source text),
2. whether an `iconLabel` field exists on rows already (PNGII/I4 style) or has to be added.

## Decision (pending that data)

Preferred approach, matching the handoff: post-process the row data after the game or SkyUI has built it, set `iconLabel` and
text fields, and let PNGII swap the label for an icon. A function hook is only used if the data cannot be reached after the fact.
**No row-modifying code is shipped in 0.1.0**; `applyToMenu` only logs a warning.

## Still to research (needs the game and, for PNGII, its author)

* Ordering against PNGII: when it reads `iconLabel` for this list.
* Effects with no spell source (race abilities, perk entry effects, diseases): the dump shows what `spell` is for each.
* Behaviour on menu reopen and on load game.

## Findings from the first in-game log (Skyrim AE 1.7.104, CommonLibSSE-NG v15.5.0)

Confirmed from `DAEDRA.log` (191 effects on a heavily modded save):

* The plugin loads on 1.7.104 and walks the player's active effect list without problems.
* `ActiveEffect::spell` gives the source for every row: blessings (`Blessing of Akatosh`, kind spell), racial and perk abilities
  (kind ability), Beastblood (ability), enchantments (kind enchantment).
* **`ActiveEffect::source` is the worn item for enchantment effects** (e.g. `Nightingale Gloves` for `Nightingale's Grasp`).
  `worn` matching is therefore viable.
* The same MGEF appears under many sources, as expected: `Fortify Archery` skyrim.esm|0x07A0FE comes from Nightingale's Sight,
  Hunter Backpack and a generic enchantment; `Increased Experience` skyrim.esm|0x0FB989 comes from Blessing of Akatosh.
* Some enchantments are runtime-created (e.g. summermyst `Increased Experience`): their form has no defining plugin, so the
  source form key is empty and only the name can match. The rule engine already handles this.
* Light plugin ids come out as local ids (`ccfsvsse001-backpacks.esl|0x000829`).
* Editor IDs are empty for nearly all records at runtime; rules must use form keys. Keyword editor IDs do work.

Scaleform: the Magic menu is SkyUI's. `_root.Menu_mc.ItemList` exists with `Entry0..Entry3` clips; the first dump ran out of
lines inside text-field properties before reaching its data. The 0.1.0-r6 dump targets `ItemList` directly.
