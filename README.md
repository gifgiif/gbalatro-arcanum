<p align="center">
  <img src="icon.png" width="96" alt="GBALATRO icon">
</p>

<h1 align="center">GBALATRO: Codex Arcanum</h1>

<p align="center">
  A feature-focused Game Boy Advance demake of <strong>Balatro</strong>, expanded
  with Alchemical Cards, Vouchers, Boss Blinds, starting decks, persistent runs,
  and playing-card modifiers.
</p>

<p align="center">
  <img alt="Platform: Game Boy Advance" src="https://img.shields.io/badge/platform-Game%20Boy%20Advance-7b55c7">
  <img alt="Language: C" src="https://img.shields.io/badge/language-C-00599c">
  <img alt="Toolchain: devkitARM" src="https://img.shields.io/badge/toolchain-devkitARM-e34f26">
  <img alt="Status: development preview" src="https://img.shields.io/badge/status-development%20preview-f0a500">
  <img alt="Codex Arcanum code: GPL-3.0" src="https://img.shields.io/badge/Codex%20Arcanum%20code-GPL--3.0-2ea44f">
</p>

> [!IMPORTANT]
> This is a non-profit fan project and a downstream modification of
> [GBALATRO/balatro-gba](https://github.com/GBALATRO/balatro-gba). It is not
> affiliated with, endorsed by, or sponsored by LocalThunk or Playstack.
> **Do not sell this project or its ROM.** Please support Balatro by purchasing
> the official game.

---

## What is this?

GBALATRO: Codex Arcanum brings a larger roguelike loop to the GBA demake while
remaining within the console's fixed memory, sprite, palette, and 240×160 display
constraints.

The expansion is built in C rather than translating the original Lua mod
line-for-line. Its Alchemical Cards use the names and mechanics of
[itayfeder/Codex-Arcanum](https://github.com/itayfeder/Codex-Arcanum), with
documented adaptations for systems that are not yet present in GBALATRO.

<p align="center">
  <img src="example.gif" width="720" alt="GBALATRO gameplay">
</p>

## Highlights

### Alchemical Cards

- All **24 Codex Arcanum Alchemical Cards** are implemented.
- Three persistent consumable slots.
- Card selection, in-blind use, clear feedback, descriptions, and selling.
- Temporary effects are removed centrally at the end of a blind.
- One lower-right Shop offer, rarity weighting, discounts, and inventory limits.
- All 24 cards appear in normal Shop rolls after bounded interaction and
  temporary-state coverage.
- New GBA-oriented 32×32 artwork with a shared 16-colour OBJ palette.

See [ALCHEMY_ADAPTATIONS.md](ALCHEMY_ADAPTATIONS.md) for the complete card table,
conditions, and differences from the PC mod.

### Expanded run systems

- **Six starting decks:** Red, Blue, Yellow, Green, Black, and Painted.
- **Twelve Vouchers** with prerequisites, persistent bonuses, and purchase
  presentation.
- **Boss Blind effects** with visible card and Joker debuffs.
- **Joker editions:** Foil, Holographic, Polychrome, and Negative.
- **Playing-card modifiers:** eight Enhancements, three Editions, and four Seals.
- **Twelve Planet Cards** that are bought for immediate, permanent poker-hand
  level upgrades.
- **SRAM run saves** for the deck, playing-card modifiers, Jokers and their
  editions, Vouchers, Alchemicals, hand levels, and starting deck.
- A real **Settings** screen and resumable run setup.
- Compile-time removable **debug menu** for development and balancing.

Detailed implementation notes:

- [Starting decks](STARTING_DECKS_GBA.md)
- [Vouchers and debug menu](VOUCHERS_AND_DEBUG.md)
- [Playing-card modifiers](PLAYING_CARD_MODIFIERS.md)
- [Planet Cards](PLANET_CARDS.md)
- [Boss Blinds](BOSS_BLINDS_GBA.md)
- [Joker editions and saves](JOKER_EDITIONS_AND_SAVES.md)
- [Joker economy and GBA price bands](JOKER_BALANCE_GBA.md)
- [Gameplay and stability audit](QA_AUDIT.md)

## Project status

This branch is playable, builds into a GBA ROM, and passes the current host-side
unit tests. It should still be treated as a **development preview**, not a final
balanced release.

Current limitations:

- Tarot and Spectral Cards are outside the current milestone. Planet Cards are
  implemented as immediate Shop purchases rather than held consumables.
- Permanent playing-card modifiers are awarded after Boss Blinds; the debug
  picker remains available only for targeted testing.
- Tarot and Spectral content remains intentionally outside this milestone.

## Controls

### Menus

| Input | Action |
|---|---|
| D-Pad | Move between buttons, cards, decks, and settings |
| A | Confirm |
| B | Back |

### During a Blind

| Input | Action |
|---|---|
| D-Pad | Navigate cards, actions, Jokers, and Alchemicals |
| A | Select a playing card or use the focused Alchemical |
| B | Deselect cards |
| L | Play hand; sell a focused owned Joker or Alchemical where available |
| R | Discard selected cards |
| Hold A + D-Pad | Reorder playing cards or Jokers |

### Shop

| Input | Action |
|---|---|
| D-Pad | Navigate controls, Joker offers, Voucher, and Alchemical offer |
| A | Buy or confirm |
| Hold B | Open the focused item's animated description |
| L | Sell a focused owned Joker or Alchemical |

### Debug menu

Debug builds use `Select+B` to open the picker. Additional shortcuts and pages
are documented in [VOUCHERS_AND_DEBUG.md](VOUCHERS_AND_DEBUG.md).

For a distributable build, disable the menu:

```sh
make DEBUG_MENU=0 -j4
```

## Building

### Requirements

- [devkitPro](https://devkitpro.org/) with devkitARM
- GNU Make
- Python 3
- Pillow for graphics conversion scripts

On a standard devkitPro installation:

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
export PATH=/opt/devkitpro/tools/bin:$PATH

make -j4
```

The resulting ROM is written to:

```text
build/gbalatro-arcanum.gba
```

Release-style build without debug controls:

```sh
make clean
make DEBUG_MENU=0 -j4
```

For Docker, Windows, macOS, Linux, formatting, and debugger setup, see
[CONTRIBUTING.md](CONTRIBUTING.md).

## Testing

Run the pure-logic host tests:

```sh
cd tests
./run_tests.sh
```

The suite currently covers the memory pool, lists, bitsets, utility functions,
all 24 Alchemical effects and dead-end guards, Vouchers and economy bounds,
starting-deck rules, Planet upgrades, Boss-rule helpers, and a deterministic
100,000-blind progression soak across Deck, Voucher, Alchemical, and Planet
rules.

For gameplay testing, open the ROM in [mGBA](https://mgba.io/):

```sh
mgba build/gbalatro-arcanum.gba
```

The ROM can also be launched through a GBA-capable emulator core on handhelds
such as the R36S.

## Repository layout

```text
source/       Game and UI implementation
include/      Public headers and fixed-size data structures
graphics/     GBA-ready indexed graphics
art/          Source artwork and generated master sheets
audio/        Music and sound assets
scripts/      Asset conversion and build helpers
tests/        Host-side unit tests
```

## Attribution

### Balatro

Balatro was designed and created by **LocalThunk** and is published by
**Playstack**. This project does not include or grant any rights to Balatro.

Buy the official game:

- [Steam](https://store.steampowered.com/app/2379780/Balatro/)
- [Nintendo Switch](https://www.nintendo.com/us/store/products/balatro-switch/)
- [PlayStation](https://store.playstation.com/en-us/concept/10010334)
- [Xbox](https://www.xbox.com/games/store/balatro/9PK087LNGJC5)
- [Apple App Store](https://apps.apple.com/us/app/balatro/id6502453075)
- [Google Play](https://play.google.com/store/apps/details?id=com.playstack.balatro.android)

### GBALATRO

This project is derived from
[GBALATRO/balatro-gba](https://github.com/GBALATRO/balatro-gba). Credit for the
base GBA implementation belongs to its maintainers and contributors.

The original music arrangement, imagery, sound, and per-Joker credits remain as
documented by the upstream project and in its
[Joker Art Discussion](https://github.com/GBALATRO/balatro-gba/discussions/69).

### Codex Arcanum

Alchemical names, descriptions, mechanics, and balance references come from
**Codex Arcanum** by **itayfeder**. The GBA port is an independent C
implementation and uses newly created compact artwork rather than copying the
mod's raster assets.

See [ALCHEMY_CREDITS.md](ALCHEMY_CREDITS.md) and the included
[GPL-3.0 license text](LICENSE_CODEX_ARCANUM_GPL-3.0.txt).

The Clanker Edition fork was reviewed for feature ideas only; no code or assets
were copied. See [VOUCHERS_AND_DEBUG.md](VOUCHERS_AND_DEBUG.md).

## Licensing

The Codex Arcanum-derived expansion code and its adaptation documentation are
provided under GPL-3.0-compatible terms and must retain the attribution above.

The upstream GBALATRO repository does not currently declare a repository-wide
software license. Original Balatro-related graphics, audio, names, and other
third-party materials remain subject to their respective owners and upstream
attributions. The included GPL-3.0 file must therefore not be interpreted as
relicensing the entire upstream project or third-party assets.

If you redistribute a build or source fork, preserve all attribution and license
documents and do not present the project as an official Balatro release.

## Contributing

Bug reports, balance notes, GBA hardware testing, and focused pull requests are
welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting code
or artwork.

When reporting a gameplay issue, include:

- ROM build or commit identifier
- emulator/device and core
- starting deck and current Ante
- owned Jokers, Vouchers, and Alchemicals
- steps to reproduce
- a screenshot or short recording when possible

---

<p align="center">
  Made for the GBA homebrew community with respect for the original creators.
</p>
