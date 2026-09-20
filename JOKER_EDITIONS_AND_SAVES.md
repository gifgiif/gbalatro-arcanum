# Joker editions and run saves

## Editions

Shop Jokers can roll one of the four Balatro editions. The current distribution
matches the unmodified Balatro shop rates: 96% Base, 2% Foil, 1.4%
Holographic, 0.3% Polychrome, and 0.3% Negative.

| Edition | Effect |
|---|---|
| Foil | +50 chips once during the independent Joker scoring pass |
| Holographic | +10 mult once during the independent Joker scoring pass |
| Polychrome | x1.5 mult once during the independent Joker scoring pass |
| Negative | +1 Joker capacity while owned, bounded by the eight-Joker GBA row |

Edition price premiums remain `$2/$3/$5/$5`. A compact `F`, two-tone sparkle,
diamond, or `N` mark is stamped directly into the Joker's private 32x32 OBJ
tiles, with no filled dark rectangle, so it does not obscure the art or consume
another OAM object or palette. The normal Joker description footer states the
actual edition bonus (`Foil: +50 chips`, `Holo: +10 mult`, `Poly: x1.5 mult`,
or `Neg: +1 slot`) instead of showing only the edition name.

Holographic Jokers do not create Alchemical Cards. Random Alchemical creation
is the GBA effect of a Purple Seal on an ordinary playing card when that card
is discarded and a consumable slot is free.

## Run save

The SRAM run format is version 9. A save written on leaving the Shop contains:

- round, Ante, money, hand size, selected deck and Blind progression;
- RNG state;
- Alchemical inventory, hand levels and use count;
- Vouchers and poker-hand play history;
- exact deck order, card ranks/suits, permanent enhancements/editions/seals,
  and current-Ante Pillar history;
- owned Joker order, IDs, editions, scoring state and persistent state.

The loader validates section tags, version, card bounds, Joker IDs, editions,
and the end tag before offering Resume. Incompatible older run saves are
ignored instead of being partially restored.

On the New Run setup screen, press `R` to open Resume, `A` to continue, and
`B` or `L` to return to New Run. Starting a new run or leaving Game Over
invalidates only the run section; Settings remain saved.
