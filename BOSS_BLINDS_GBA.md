# Boss Blinds — GBA implementation

All Boss and Showdown Blinds now have gameplay effects. The implementation
keeps the original rule where it fits the demake and uses bounded GBA state.

| Blind | GBA effect |
|---|---|
| The Hook | Discards up to 2 random held cards after a played hand |
| The Ox | At blind start, locks onto the run's single most-played poker hand (stable first-hand tie-break); playing that hand sets money to $0 |
| The House | Initial hand is face down |
| The Wall | 4x base score requirement |
| The Wheel | Each drawn card has a 1-in-7 face-down chance |
| The Arm | Removes one stored bonus level from the played hand, minimum base level |
| The Club | Clubs are debuffed |
| The Fish | Cards drawn after a played hand are face down |
| The Psychic | Exactly 5 cards must be played |
| The Goad | Spades are debuffed |
| The Water | Starts with 0 discards |
| The Window | Diamonds are debuffed |
| The Manacle | -1 hand size for the blind |
| The Eye | A poker hand type cannot be repeated |
| The Mouth | After the first hand, only that poker hand type may be played |
| The Plant | Jack, Queen, and King cards are debuffed |
| The Serpent | Draws up to 3 cards after a play or discard |
| The Pillar | Cards played during the current Ante are debuffed |
| The Needle | Starts with 1 hand |
| The Head | Hearts are debuffed |
| The Tooth | Loses $1 per played card, clamped at $0 |
| The Flint | Base chips and mult are halved, minimum 1 |
| The Mark | Jack, Queen, and King cards are drawn face down |
| Amber Acorn | Joker order is shuffled and hidden until the first hand |
| Verdant Leaf | Playing cards are debuffed until one Joker is sold |
| Violet Vessel | 6x base score requirement |
| Crimson Heart | One random Joker is disabled for each hand |
| Cerulean Bell | One random held card is forced selected after each draw |

## Visual language

- Debuffed playing cards receive a red X and contribute no base chips or
  Alchemical card effects.
- Face-down cards use the active deck back.
- Alchemical playing-card modifiers use compact coloured pips along the bottom
  edge. Focusing a modified card displays short labels:
  `PO ST GL GO LU OI BO` (Poly, Steel, Glass, Gold, Lucky, Oil, Borax).
- Disabled Jokers remain visible but shrink to 80% for the duration of their
  disabled state. This keeps the cause of a missing effect readable without
  allocating another GBA overlay sprite.

## Intentional adaptations

- Amber Acorn reveals the shuffled Joker row after the first hand. This gives
  the same uncertainty without permanently consuming additional face-down
  Joker graphics and OAM.
- The Arm changes the persistent bonus-level storage currently used by the
  demake. Base poker-hand values never fall below level 1.
- The Ox chooses its target once at blind start. It does not move to another
  hand after play counts change, and a tie never penalizes several hand types.
- The Serpent draws at most three cards when hand/deck capacity is lower.
- All counters, masks, money changes, hand sizes, and array operations are
  clamped to their existing GBA limits.
