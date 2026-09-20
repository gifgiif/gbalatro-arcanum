# Playing-card modifiers

Ordinary playing cards have three permanent, independently saved modifier
slots. They are separate from Codex Arcanum's one-blind temporary flags.

## Enhancements

| Enhancement | GBA effect |
|---|---|
| Bonus | +30 chips when scored |
| Mult | +4 mult when scored |
| Wild | Counts as every suit; still receives suit Boss debuffs |
| Glass | x2 mult when scored; 1-in-4 chance to be destroyed after scoring |
| Steel | x1.5 mult while held in hand |
| Stone | Has no rank or suit, always scores +50 chips when played |
| Gold | Earns $3 when held at a successful blind end |
| Lucky | 1-in-5 +20 mult and independent 1-in-15 +$20 rolls when scored |

## Editions

| Edition | GBA effect |
|---|---|
| Foil | +50 chips when scored |
| Holographic | +10 mult when scored |
| Polychrome | x1.5 mult when scored |

## Seals

| Seal | GBA effect |
|---|---|
| Gold | +$3 when scored |
| Red | Retriggers played scoring and held-in-hand effects, including Steel and compatible Joker events, once |
| Blue | Raises the last played poker hand one level when held at blind end |
| Purple | Creates a random shop-enabled Alchemical when discarded if a slot is free |

Purple and Blue are documented GBA adaptations. Tarot consumables are not yet
present, so Purple creates a random shop-enabled Alchemical. Planet Cards are
implemented as immediate Shop purchases rather than held consumables, so Blue
applies its level directly and writes into the same persistent hand-level
storage used by purchased Planets.

Until Tarot acquisition is implemented, defeating a Boss Blind grants one
permanent modifier to a random card and reports the exact card/modifier on the
cash-out screen. Safe enhancements are weighted most heavily; seals and
editions are less common. This supplies modifiers in normal runs without using
the shared lower-right Planet/Alchemical Shop slot.

Enhancements use a small `E` badge, editions use a patterned top marker, and
seals use a coloured top-right stamp. Focusing a card prints the exact compact
modifier labels. Modifier fields and current deck contents are stored in SRAM
save format 9.

For targeted testing, build with `DEBUG_MENU=1`,
select a playing card, open `Select+B`, and use the `ENHANCE`, `EDITION`, or
`SEAL` pages.
