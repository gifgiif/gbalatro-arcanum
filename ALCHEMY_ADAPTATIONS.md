# Alchemical Cards: GBA rules and adaptations

All 24 cards are usable only while a blind is active and occupy one of three
consumable slots. Common cards cost $4, Uncommon cards cost $5, and Rare cards
cost $6. Tiers also control shop weighting (5:3:1). The initial stock of the
first shop guarantees a shared Planet/Alchemical offer; rerolls and later shops
use the normal 75% offer chance. Of
generated special offers, 40% are Planets, for effective later-shop rates of
30% Planet, 45% Alchemical, and 25% empty.

| Card | Tier | Selection | GBA effect |
|---|---|---:|---|
| Ignis | Common | None | Gain 1 discard. |
| Aqua | Common | None | Gain 1 hand. |
| Terra | Common | None | Reduce the current blind requirement by 15%, minimum 1. |
| Aero | Common | None | Draw up to 4 cards into the current hand, matching Codex Arcanum. The temporary overflow may reach the GBA array limit of 16 cards, but it does not enlarge later hands. Aero is not consumed when the deck is empty or the hand already contains 16 cards. |
| Quicksilver | Uncommon | None | Gain exactly 2 hand size and immediately fill the new space; reset at blind end. It is not consumed when fewer than two slots remain before the 16-card GBA hard limit. |
| Salt | Common | None | Tags do not exist in GBALATRO, so gain $4, a compact guaranteed tag-value substitute without a guaranteed base-price profit. |
| Sulfur | Uncommon | None | Set remaining hands to 1 and gain $2 for every hand removed. This keeps the cash-out useful without making a $5 card an almost automatic large profit at the normal four-hand start. |
| Phosphorus | Common | None | Move the entire discard pile back to the deck and shuffle it. If a draw stopped because the deck was empty, immediately refill the hand from the restored deck. This also prevents a false loss when Phosphorus is the only legal rescue, including against The Psychic. |
| Bismuth | Uncommon | 1–2 | Temporary Polychrome: each affected scored card gains x1.5 mult. It does not stack with an already-Polychrome edition, and currently Boss-debuffed cards are rejected. |
| Cobalt | Rare | 1–5 | Permanently add up to 2 upgrades to the selected poker hand (20-upgrade storage cap, displayed as LV 21 because every hand starts at LV 1). Each hand uses its own Balatro-style chip/mult level increment. |
| Arsenic | Uncommon | None | Swap remaining hands and discards. |
| Antimony | Rare | None; retriggerable Joker required | The leftmost Joker's successful effects trigger one additional time for this blind. A second use stacks one more retrigger. Base/Negative Pareidolia, Shortcut, and Four Fingers are passive integrations and reject Antimony instead of consuming it for no result. This represents temporary Negative copies without exceeding the fixed Joker UI. |
| Soap | Common | 1–3 | Return selected cards to the deck, shuffle, and draw the same number of replacements. |
| Manganese | Uncommon | 1–4 | Temporary Steel: each affected clean active card held after playing applies x1.5 mult. Boss-debuffed, permanently enhanced, and already temporarily enhanced selections are rejected. |
| Wax | Rare | Exactly 1 | Create two temporary copies in hand. If the hand is full, its capacity expands just enough for both copies (up to the 16-card hard limit); that capacity and both copies disappear at blind end. Wax is not consumed if the hard limit cannot fit both copies. |
| Borax | Common | 1–4 | Temporarily change selected ranked, non-Wild cards to the most common suit in the whole owned deck. Stone and Wild cards are rejected because their poker-hand suit would not change. |
| Glass | Uncommon | Exactly 1 | Temporary Glass: double mult when the affected clean active card scores. The PC break chance is omitted for stability, so the one-card cap replaces that missing destruction risk. |
| Magnet | Uncommon | Exactly 1 | Search the deck and draw up to 2 cards of the selected rank. Stone cards are rejected because they have no gameplay rank. The draw may temporarily overflow the normal hand-size target, up to the 16-card GBA hard limit, so the card remains usable after the automatic opening draw fills the hand. |
| Gold | Uncommon | 1–4 | Temporary Gold: gain $2 for each affected clean, active, non-copy card still held in hand at blind end. A Red Seal retriggers that held-card payout for $4. Played, discarded, Boss-debuffed, enhanced, and Wax-copy cards do not pay; an all-invalid selection is rejected without consuming Gold. |
| Silver | Uncommon | 1–4 | Temporary Lucky: when an affected clean active card scores, 1-in-5 adds +10 mult and 1-in-15 grants $5. Values are reduced for the shorter GBA economy; Boss-debuffed, permanently enhanced, and already temporarily enhanced selections are rejected. |
| Oil | Common | None | Remove Boss debuffs and face-down state from all cards currently in hand, and protect those cards from being re-debuffed or turned face-down again during this blind. |
| Acid | Rare | Exactly 1 | Temporarily remove every owned card of the selected rank from hand, deck, and discard; Stone cards are rejected because they have no gameplay rank. Removed cards return to the deck at blind end. The current hand is refilled from the remaining deck after use. The card is rejected if it would leave no playable card, or fewer than five accessible cards against The Psychic. |
| Brimstone | Rare | None | Requires an active Joker. Gain 2 hands and 2 discards, then mute the first currently active Joker for the blind (the GBA equivalent of debuffing it). Later uses mute the next active Joker, up to the three-entry GBA temporary-state limit. It is not consumed if no active Joker can pay the downside or if hands/discards cannot increase. |
| Uranium | Rare | Exactly 1 | Copy the selected card's enhancement, seal, edition, and temporary Alchemical flags to up to 3 currently unmodified cards in hand. Suit is copied only when the selected card currently carries Borax, and every target retains its own original suit for blind-end restoration. |

Steel, Glass, Gold, and Lucky are mutually exclusive temporary enhancement
alternatives. This mirrors the single enhancement slot of a Balatro playing
card and prevents silent no-op consumption or excessive stacking. Temporary
flags, Acid storage, Wax copies, Joker duplication/muting, and the
Quicksilver hand-size deltas are cleared by one centralized blind-end routine.
Deck, hand, discard, Acid storage, and temporary copies use explicit capacity
checks; no operation may exceed the 64-card owned-card pool or 16-card visible
hand. Wax copies participate in temporary scoring effects but cannot mint
permanent rewards from Gold or Blue Seals, Purple Seals, or Alchemical Gold.
Selling a Joker also clears any Antimony, Brimstone, or Boss pointer to
that object before its fixed-pool slot can be reused.

Boss and Brimstone disable state is shared by both scored Joker effects and
passive integrations. Pareidolia, Shortcut, and Four Fingers therefore stop
changing hand evaluation while their Joker is disabled, and become active
again when the blind-end cleanup restores them.

The normal Shop pool contains all 24 effects. Cobalt level saturation,
Antimony target lifetime, temporary enhancement exclusivity, Borax tie and
restoration rules, Silver economy, and Uranium clean-target copying have
dedicated boundary coverage. Playing-card modifiers have coloured bottom-edge
markers; temporary Wax copies also have a separate top marker. Debuffed Boss
Blind cards display a red X.
Uranium refuses to consume itself when the selected card has no modifier or
there is no clean copy target.

## Controls

- Main menu: Up/Down chooses **New Run** or **Settings**, A confirms.
- Settings: Up/Down selects game speed, music volume, sound volume, or card
  sprites; Left/Right changes the selected value; B returns to the main menu.
- During a blind: Up from the hand enters the shared Joker/Alchemical row,
  Left/Right chooses an owned card, Down returns to the hand, A uses the
  selected Alchemical, and L sells it for half its base price.
- Cards with selection requirements use the normal hand selection first.
- Shop: one Planet or Alchemical offer occupies the shared lower-right slot
  below the Joker row; the Voucher occupies the lower-left slot. Next Round and
  Reroll form their own vertical control column. Down from any Joker offer goes
  directly to the special offer; Left/Right connects the lower merchandise
  cards. Press A to buy. A Planet applies immediately instead of entering the
  three-card Alchemical inventory.
  Hold B on the offer to
  open the same animated description panel used by Jokers; it contains the
  name, cost, rarity, and a compact plain-language effect. Held Alchemicals are
  included in the upper inventory row and can be sold with L. The three-card
  held limit is checked before charging money.

## GBA card art

The final 6x4 ImageGen source sheet is stored at
`art/imagegen/alchemical_cards_imagegen_v3.png`. It preserves the 24-card
registry order and uses a single contained card silhouette without external
shadows, rarity bands, or transparent holes inside the card. The deterministic
`scripts/process_alchemical_gfx.py` converter crops the green gutters, reduces
each complete card to 32x32, and maps it to the shared 16-colour OBJ palette.
