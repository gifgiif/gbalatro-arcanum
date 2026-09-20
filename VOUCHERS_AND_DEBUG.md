# Vouchers and Debug Menu

This implementation was written independently for GBALATRO. The
`FuriousTroller/balatro-gba-clanker-edition` fork was reviewed for feature
ideas and GBA interaction patterns; no source code or graphical assets were
copied from it.

## Voucher rules

One Voucher is offered in the lower-left Shop slot per Ante. It costs `$10`,
persists when the Shop is rerolled, and does not return after purchase until
the next Ante. Upgrade Vouchers become eligible only after their base Voucher
has been purchased.

| Base Voucher | Effect | Upgrade | Upgraded effect |
| --- | --- | --- | --- |
| Overstock | +1 Joker offer per Shop | Overstock Plus | +1 additional Joker offer |
| Clearance Sale | Jokers, Planets and Alchemicals cost 25% less | Liquidation | 50% less |
| Reroll Surplus | Rerolls cost $2 less | Reroll Glut | Another $2 less; minimum $1 |
| Grabber | +1 hand each Blind | Nacho Tong | +1 additional hand |
| Wasteful | +1 discard each Blind | Recyclomancy | +1 additional discard |
| Blank | No direct effect; unlocks Antimatter | Antimatter | +1 Joker capacity |

All prices round up. Hands and discards are recalculated when a Blind starts,
so a Voucher bought in the preceding Shop applies immediately to that Blind.
Voucher ownership and the current Ante offer are stored in SRAM save data.

## Shop controls

- `Down` from **Next Round** selects **Reroll**; the two controls form a
  separate vertical column.
- `Down` from any Shop Joker selects the lower-right Alchemical directly.
- `Up` from the Alchemical returns to the rightmost Shop Joker.
- `Right` from Reroll enters the merchandise row at the Voucher.
- `Left/Right` moves directly between Voucher and Alchemical when both exist.
- `B` opens the standard animated item description.
- `A` buys an item. A bought Voucher displays its full description and waits
  for another `A` press before returning to the Shop.

## Debug menu

The menu is disabled by default. Enable it explicitly for a development ROM:

```sh
make DEBUG_MENU=1
```

Runtime controls:

- `Select+B`: open the picker.
- `L/R`: switch between Alchemical and Voucher pages.
- `Up/Down`: choose an item.
- `A`: grant the selected item.
- `B`: close the picker.
- `Select+A`: add `$100`.
- `Select+Down`: satisfy the current Blind score requirement.
- `Select+L`: add one hand and one discard.

Normal game input is suspended while the picker is open, preventing debug
button presses from buying, selling, or playing an item underneath it.

## Original GBA art

The 12 Voucher concepts were generated specifically for this project and then
processed by `scripts/process_voucher_gfx.py` into compact ticket-shaped cards
on a shared 16-colour OBJ palette. Each card is one contained silhouette with
no external shadow or coloured edge strip. Both 32x32 sheets intentionally
contain the same art: focus is communicated by the existing sprite raise
rather than a second frame. Source and processed art:

- `art/imagegen/voucher_cards_imagegen_v2.png`
- `graphics/voucher_gfx.png`
- `graphics/voucher_focus_gfx.png`

The generation prompt requested a 6x2 sprite sheet of twelve distinct,
license-safe pixel-art voucher cards on a flat chroma background, representing
the six base/upgrade pairs listed above. The final GBA images are newly created
assets and do not derive from the fork's graphics.
