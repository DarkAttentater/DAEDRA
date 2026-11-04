# Writing rules

Rule files live in `Data/SKSE/Plugins/DAEDRA/*.json`. Files load alphabetically (case-insensitive); files whose
name starts with `_` are skipped. Comments (`//`, `/* */`) are allowed. Point your editor at `schema/daedra-rules.schema.json`
for validation and completion.

```json
{
  "$schema": "https://raw.githubusercontent.com/DarkAttentater/DAEDRA/main/schema/daedra-rules.schema.json",
  "version": 1,
  "rules": [
    { "id": "my.rule", "priority": 0, "match": { ... }, "assign": { ... } }
  ]
}
```

## Choosing the winner

Among rules that match a row, the highest `priority` wins; on a tie the later rule wins (later file, or later in the file).
The winning rule's whole `assign` block is used; blocks from several rules are **not** merged.

## Finding the values

Set `dumpActiveEffects = true` in `DAEDRA.ini`, open the Magic menu, then read
`Documents/My Games/Skyrim Special Edition/SKSE/DAEDRA.log`. Each effect shows its effect form, source form,
source name and kind, keywords and caster, in the format you paste into rules.

## Forms

`"Plugin.esp|0xLOCALID"`: the plugin that **first defines** the record, plus the id without the load-order byte. For light
plugins use the 3-digit local id. Plugin names are case-insensitive. A record overridden by another plugin is still matched by
its original definition.

## Match keys

| Key | Meaning |
|---|---|
| `plugin` | Plugin that defines the source form |
| `effect.form` / `edid` / `plugin` | The magic effect (MGEF). `edid` is only filled for records that expose an editor ID at runtime, which most do not; prefer `form` |
| `effect.keywords` (all) / `keywordsAny` | Keyword editor IDs on the MGEF |
| `effect.archetype`, `effect.actorValue`, `effect.detrimental` | Archetype name (`ValueModifier`, ...), actor value name, detrimental flag |
| `source.form` / `edid` / `plugin` | The spell, enchantment, potion, ingredient or scroll that produced the effect |
| `source.kind` | `spell enchantment potion ingredient ability power lesserPower disease scroll staff unknown` (string or list) |
| `source.name` | String (exact) or `{ "exact" / "contains" / "regex" }`, all case-insensitive. Regex is compiled once at load; keep it simple |
| `source.keywords`, `source.spellType` | Keywords on the source; spell type name |
| `caster` | `"player"`, `"follower"`, `"other"`, or `{ "form": "..." }` |
| `worn` | `{ "form", "keywords" }` of the item recorded in `ActiveEffect::source`. The worn item behind a gear effect; see `hook-research.md` |
| `anyOf`, `allOf` (lists), `not` | Combine match objects; nesting limited to 8 levels |

A `match` with no usable keys is rejected so a typo cannot create a rule that hits every row.

## Assign keys

| Key | Effect on the Active Effects row |
|---|---|
| `iconLabel` | Sets the row's `iconLabel`, which icon mods such as PNGII map to an icon |
| `iconSource` | Sets the row's `iconSource` |
| `sourceText` | Replaces the text in the Source column |
| `nameText` | Replaces the effect name |
| `iconFromWorn` | Row takes the icon of the worn item that carries the effect (see below) |
| `iconFromSource` | Row takes the icon of the spell, ability or other source that produced the effect |
| `iconAsForm` | Row takes the icon of an explicit form, `"Plugin.esp\|0xLOCALID"` |

`extraText`, `sortGroup`, `color` and `hide` are parsed and validated but are **not applied yet**.

When a row borrows another form's icon, DAEDRA also copies that form's type and keywords onto the row (`iconMimicType` in
`DAEDRA.ini`). For armor it also reuses the item's own icon label. Those labels are learned from the Inventory menu whenever you
open it, and cached in `Data/SKSE/Plugins/DAEDRA/cache/` so they are available the next time you open Active Effects.

## Errors

Every problem is logged with file name, rule index and rule id, for example
`DICK.json: rule 3 [dick.mara].match.effect.form: bad form 'x', expected "Plugin.esp|0xHEX"`. A bad rule is skipped; the
others keep working. Set `strict = true` to also reject rules containing unknown keys.

## Cookbook (placeholders, replace the forms with ones from your log)

Same effect, different source:

```json
{ "id": "xp.akatosh", "match": { "effect": { "form": "Skyrim.esm|0x0XXXXX" }, "source": { "name": "Blessing of Akatosh" } },
  "assign": { "iconLabel": "deity_akatosh" } },
{ "id": "xp.enchant", "match": { "effect": { "form": "Skyrim.esm|0x0XXXXX" }, "source": { "kind": "enchantment" } },
  "assign": { "iconLabel": "skill_experience" } }
```

Any disease the player carries, from any mod:

```json
{ "id": "any.disease", "match": { "source": { "kind": "disease" } }, "assign": { "iconLabel": "vamp_disease" } }
```

Follower-cast buffs:

```json
{ "id": "follower.buff", "match": { "caster": "follower", "effect": { "detrimental": false } }, "assign": { "extraText": "From follower" } }
```

## Registering rules from another plugin

See `api/DAEDRA_API.h`.

## `iconFromWorn`: effects that come from gear

`"assign": { "iconFromWorn": true }` points the Active Effects row at the worn item that carries the effect (`ActiveEffect::source`),
so any icon rule that applies to that item in PNGII (form, keywords, DICK's per-item icons) applies to the effect row too. The
effect form stays available to DAEDRA as `daedraFormId` on the row. A low-priority catch-all gives every gear effect its item's icon:

```json
{ "id": "any.gear", "priority": -10, "match": { "worn": {} }, "assign": { "iconFromWorn": true } }
```

Specific rules still win over it (the winning rule's whole `assign` is used, so repeat `iconFromWorn` in them if you want both).
Needs an icon mod such as PNGII (with DICK's rules) to draw the icons. DAEDRA reapplies a few times just after the menu opens,
because SkyUI and PNGII rebuild the rows then.
