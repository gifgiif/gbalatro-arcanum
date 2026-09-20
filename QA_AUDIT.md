# Gameplay and stability audit

This file records the failure modes that must be considered when changing the
run loop. It is intentionally organized around transitions and invariants,
rather than around individual screenshots.

## Hard resource invariants

- At most 64 owned playing cards exist across deck, hand, played cards,
  discard, and temporary Acid storage.
- At most 16 playing cards are visible at once.
- A normal blind can display at most 16 playing cards, 8 owned Jokers, and 3
  held Alchemicals: 27 of the 28 `SpriteObject` pool entries.
- A full Shop can display at most 8 owned Jokers, 4 offers, 3 held
  Alchemicals, one Voucher, and one Planet/Alchemical special: 17
  `SpriteObject` entries.
- Owned, sold/expiring, and Shop Jokers share a 12-object pool. Selling or
  buying moves ownership; it must not duplicate the object.
- Every constructor either returns a complete object or rolls back every pool,
  OAM-layer, palette, and sprite allocation.

## Transaction rules

An action that fails validation must not charge money, consume a card, grant a
partial effect, or leave a focused/dangling object.

- Joker, Voucher, Planet, and Alchemical purchases validate their target and
  capacity before subtracting money.
- Planet levels are applied before payment; a capped level rejects the purchase.
- Alchemical scalar effects commit from a local copy only when their complete
  effect changes state.
- Compound Alchemical effects preflight card capacity, hand capacity, legal
  follow-up actions, and required Joker targets.
- Failed object construction restores Shop availability and leaves the current
  run playable.

## Alchemical dead-end checks

- Aero rejects an empty deck or a 16-card hand.
- Quicksilver and Wax reject the 16-card hard limit before changing hand size
  or allocating copies.
- Phosphorus can rescue an empty deck by shuffling discards and immediately
  resuming the interrupted draw. The Psychic additionally requires enough
  restored cards to reach five.
- Soap requires enough deck cards for every selected replacement.
- Magnet requires a ranked selection, a matching card in the deck, and visible
  hand capacity.
- Acid may not remove the last playable card, or leave fewer than five
  accessible cards against The Psychic.
- Brimstone requires both resource headroom and a currently active Joker to
  mute.
- No-op effects remain held and display a reason; every held Alchemical can
  also be sold.

All 24 effects are available in paid Shop rolls. The interaction-heavy group
(Cobalt, Antimony, Manganese, Borax, Silver, and Uranium) additionally checks
level saturation, live Joker targets, one-slot temporary enhancements,
deterministic suit selection/restoration, bounded Lucky payouts, and clean
copy targets.

## Boundary cleanup

- New Run clears score interpolation, card animation, drag input, Boss state,
  Joker iterators, Shop descriptions, feedback timers, and selection cursors.
- Blind start clears per-hand scoring and input state before drawing.
- Blind end is the single cleanup point for temporary Alchemical card flags,
  Wax copies, Acid storage, Uranium backups, temporary hand size, Antimony
  retriggers, Brimstone mutes, and Boss visuals/debuffs.
- Shop exit destroys its special, Voucher, and held-inventory presentation
  objects and erases their text.
- Save loading is all-or-nothing. Invalid enums, capacities, prerequisite
  chains, card modifiers, Joker counts, and impossible Blind state are rejected.

## Automated coverage

Host tests cover:

- bitsets, lists, fixed pools, and foreign-pointer rejection;
- bounded number formatting and score arithmetic;
- all 24 Alchemical definitions, use conditions, inventory limits, scalar
  transactions, Wax/Acid/Phosphorus rescue limits, permanent-level saturation,
  temporary-enhancement exclusivity, suit ties, and clean modifier copying;
- all 12 Vouchers, prerequisites, monotonic discounts, non-zero rerolls, and
  integer extremes;
- all starting-deck rule modifiers;
- all 12 Planets, secret unlocks, dual Neptune upgrade, and level caps;
- Boss rule selection, repetition, hand-size, and payout helpers;
- a deterministic 100,000-blind economy/progression soak across Deck,
  Voucher, Alchemical, and Planet rules.

The complete source tree is also checked with GCC `-fanalyzer`, and the host
tests are run with AddressSanitizer and UndefinedBehaviorSanitizer.

## Manual release checklist

Automation cannot verify GBA presentation timing or controller feel. Before a
release ROM, manually check:

1. New Run and every starting deck preview.
2. Every Settings row, both directions, Save, and Cancel.
3. Shop navigation from each Joker to the lower-right special, between the
   special and Voucher, and back to the controls.
4. Failed and successful purchases, descriptions, selling, reroll, and Shop
   exit cleanup.
5. At least one selection-free, selected-card, draw, deck-mutating, economy,
   and Joker-mutating Alchemical.
6. Planet purchase followed by scoring the upgraded hand in a later blind.
7. Small Blind, Big Blind, a normal Boss, and an Ante 8 Showdown.
8. Flush, Straight, five-card scoring, card editions/seals, Joker editions, and
   score values large enough to use K/M/B suffixes.
9. Save at Blind Select, cold restart, Resume, then Shop → Blind → Shop.
10. A release build on the same emulator core used by the R36S.
