# Planet Cards

The Shop implements all twelve Balatro Planet Cards. A Planet has a base price
of $6, shares the lower-right special-offer slot with Alchemical Cards, and
applies immediately when purchased. It never occupies one of the three held
Alchemical slots.

The first Shop's initial stock guarantees a Planet/Alchemical offer. Rerolls
and later Shops have a 75% chance to generate a special offer; 40% of generated
special offers are Planets. This gives normal rolls effective rates of 30%
Planet, 45% Alchemical, and 25% empty. Shop discount Vouchers may reduce the $6
base price.

## Permanent hand upgrades

Every poker hand starts at displayed level 1. Internally, the save stores the
number of upgrades, beginning at 0. A purchased Planet adds one upgrade and the
next hand-value calculation uses the corresponding Balatro-style chip and mult
increments.

| Planet | Poker hand | Chips per upgrade | Mult per upgrade |
|---|---|---:|---:|
| Pluto | High Card | +10 | +1 |
| Mercury | Pair | +15 | +1 |
| Uranus | Two Pair | +20 | +1 |
| Venus | Three of a Kind | +20 | +2 |
| Saturn | Straight | +30 | +3 |
| Jupiter | Flush | +15 | +2 |
| Earth | Full House | +25 | +2 |
| Mars | Four of a Kind | +30 | +3 |
| Neptune | Straight Flush and Royal Flush | +40 | +4 |
| Planet X | Five of a Kind | +35 | +3 |
| Ceres | Flush House | +40 | +4 |
| Eris | Flush Five | +50 | +3 |

Neptune updates both Straight Flush and Royal Flush because GBALATRO exposes
Royal Flush as a separate display hand while Balatro treats it as the same
Planet progression.

Planet X, Ceres, and Eris enter the Shop pool only after their associated secret
hand has been played at least once during the run. A Planet whose affected hand
is already capped is excluded from generation and cannot charge the player.

The GBA implementation stores at most 20 upgrades per hand. Since the starting
hand is displayed as level 1, the highest displayed value is LV 21. This is a
bounded-array adaptation, not a promise of a displayed LV 20 cap.

## Persistence and related effects

Hand upgrades persist across blinds and are included in SRAM run saves. Loading
a run restores all hand levels and secret-hand play counts.

Blue Seals apply their hand upgrade directly at a successful blind end rather
than creating a held Planet. Cobalt uses the same hand-level storage. The Arm
Boss Blind may permanently remove one stored upgrade from the hand played
against it, but never reduces a hand below its base level.
