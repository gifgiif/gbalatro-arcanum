# ImageGen sprite sources

Mode: OpenAI built-in ImageGen.

## Alchemical cards

Source: `alchemical_cards_imagegen_v3.png`

Prompt: redesign the supplied 24-card reference as an exact 6x4 GBA pixel-art
sheet in the original registry order. Use one centered recognizable symbol per
card, warm parchment, one thin dark navy border, clipped corner ornaments, and
flat `#00ff00` gutters. No text, rarity bands, coloured top/bottom strips,
external or offset shadows, glow, transparent holes inside cards, magenta,
watermarks, or reordered/missing cards.

## Vouchers

Source: `voucher_cards_imagegen_v2.png`

Prompt: redesign the supplied 12-Voucher reference as an exact 6x2 GBA
pixel-art sheet in the original registry order. Use one centered recognizable
symbol, pale parchment, one thin dark navy border, small ticket notches, and
flat `#00ff00` gutters. No text, ribbons outside the card, coloured edge
strips, external or offset shadows, glow, transparent holes inside cards,
magenta, watermarks, or reordered/missing cards.

The processing scripts remove only edge-connected chroma, resize each complete
card to a 32x32 cell, and map it to the shared 16-colour OBJ palette.
