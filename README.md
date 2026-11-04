# DAEDRA

**D**ark's **A**ctive **E**ffect **D**isplay & **R**ule **A**ssistant (formerly ESI, Effect Source Injector)

An SKSE plugin that lets rule files match a Magic menu **Active Effects** row on the effect **and the thing that produced it**, so
rows that share one generic icon (shrine blessings, Beastblood, enchantments that reuse the same effect) can be told apart.

> **Status: 0.1.0, first release.** Tested in game on Skyrim AE 1.7.104 with SkyUI and PNGII. SE and other AE versions use the
> same Address Library build but have not been tested.

## What works

* Loads on SE and AE through Address Library (CommonLibSSE-NG). No VR.
* JSON rule engine with priorities, combinators (`anyOf` / `allOf` / `not`), form / keyword / name / kind / caster / worn-item
  matching, and clear per-rule error messages. Covered by unit tests (`ctest`).
* Rewrites Active Effects rows in the Magic menu: `iconLabel`, `iconSource`, `sourceText` (Source column), `nameText`, and
  icon borrowing from the worn item (`iconFromWorn`), the source spell (`iconFromSource`) or any form (`iconAsForm`).
* Armor effects can show the same icon as the item itself, including the generic armor icons, learned from the Inventory menu.
* Diagnostic dump of active effects and of the Magic menu's Scaleform tree, optional hotkeys, hot reload of rule files.
* A small public API so other plugins can register rules (`api/DAEDRA_API.h`).

`extraText`, `sortGroup`, `color` and `hide` are accepted in rules but not applied yet.

## Requirements

SKSE64, Address Library for SKSE Plugins, SkyUI. To draw custom icons you also need an icon mod that maps row labels or forms to
icons, such as PNGII. DAEDRA does not draw anything itself.

## Install

Copy the `SKSE` folder from the release into `Data` (or install with Vortex). Settings: `SKSE/Plugins/DAEDRA.ini`.
The release contains no active rules. `SKSE/Plugins/DAEDRA/_example.json` has examples, including one that gives every gear
effect its item's icon: copy it to a name without the leading `_` to use it.
Log: `Documents/My Games/Skyrim Special Edition/SKSE/DAEDRA.log`.

## Writing rules

See [docs/rule-authoring.md](docs/rule-authoring.md). JSON Schema: [schema/daedra-rules.schema.json](schema/daedra-rules.schema.json).

## Building

Needs Visual Studio 2022 (C++ workload), CMake 3.21+, and vcpkg with `VCPKG_ROOT` set.

```
git clone --recurse-submodules <this repo>
cmake --preset tests  && cmake --build --preset tests  && ctest --preset tests    # rule engine only
cmake --preset plugin && cmake --build --preset plugin                            # full DLL
```

If you cloned without submodules: `git submodule update --init`.

## Licence

GPL-3.0-or-later, see `LICENSE`. DAEDRA links [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) (GPL-3.0-or-later with a modding exception), which is why it uses the same licence. Also uses spdlog, nlohmann/json
and doctest.

## Shipping rules from your own mod (for example DICK)

DAEDRA ships no rules of its own. A mod that wants its own icons or labels drops a JSON file into
`SKSE/Plugins/DAEDRA/` (for example `SKSE/Plugins/DAEDRA/DICK.json`) and lists DAEDRA as a requirement. A rule can match an effect
**and** the source that produced it, so identical effects from different sources get different results:

```json
{ "version": 1, "rules": [
  { "id": "dick.akatosh",  "match": { "effect": { "form": "Skyrim.esm|0x0FB989" }, "source": { "form": "Skyrim.esm|0x0FB988" } },
    "assign": { "iconLabel": "deity_akatosh" } },
  { "id": "dick.circlet",  "match": { "effect": { "form": "Skyrim.esm|0x0FB989" }, "source": { "kind": "enchantment" } },
    "assign": { "iconLabel": "skill_experience" } }
] }
```

Row icons come from the `iconLabel` that DAEDRA writes onto the Active Effects row, so a mod such as PNGII that maps labels to
icons does the drawing.
