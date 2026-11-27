# actors16 people: physical descriptions

Rows 0-15 of `actors16.png` are the people. This file describes each row so that
per-NPC face portraits (`dialogue_show(NPC_ID, ...)`) can match the sprite in
sex, hair colour and style, and head covering. Rows 16-31 are creatures and are
not covered here.

## How to read this

The sheet is 1-bit, so colour is never stored. Hair colours below are readings of
tone, using this ramp (darkest to lightest):

| Sprite tone | Read as |
| --- | --- |
| Solid black, a few white glints | black |
| Dense checker | dark brown |
| Diagonal hatching | medium brown |
| Mostly white with a few dark specks | blond to white |
| Mostly white with sparse diagonal strokes | silver-gray |

The world is mostly black, so black at the edge of a sprite is invisible and only
the white shapes read. Black inside a white shape (eyes, a hat band, a parting)
does show. Each white shape keeps a black border so the sprite stays legible over
busy tiles such as grass.

Frame coordinates use y = 0 at the top of the 16 px frame; "S frame" is the
south-facing idle frame (column 6).

## Shared anatomy

- **Head:** a round 12 x 10 px silhouette in every row's south frame. Hair, bands
  and brows are drawn inside it. Exceptions are the pointed hood (row 3) and the
  hat brim (row 12), which change the outline. Long hair extends past the head in
  the north and side frames.
- **Face:** two vertical dark eyes, one pixel wide and two tall. No nose is drawn.
  Rows 4 and 7 add an eyepatch and a moustache, and row 11 is the only one with a
  mouth. Small outlined ears sit at eye height on each side, except under the
  hood and the hat.
- **Skin:** plain white in every row. No skin-tone variation is encoded.
- **Age and expression:** not encoded. Every face is neutral except row 11, which
  smiles.
- **Two body templates.** Rows 0-7 have **short hair** and a **short belted tunic**
  with the legs visible below. Rows 8-15 have **long hair** and a **dress** with a
  flared hem. Long hair falls past the shoulders to about mid-back, and it is
  visible from the back and side as texture that continues below the head.
  These two templates are what make rows 0-7 read as male and rows 8-15 as female.
- **Head coverings:** row 3 a white pointed hood, row 5 a white headband, row 12 a
  wide-brimmed hat and row 14 a polka-dot kerchief. Row 7 is bald.
- **Profiles:** west frames mirror east frames exactly, so an asymmetric feature
  such as the eyepatch on row 4 appears on the near eye in both profiles.

## Summary

| Row | Sex | Hair | Head covering | Distinguishing detail | Used by |
| --- | --- | --- | --- | --- | --- |
| 0 | male | medium brown, short, fringe | none | none | player (`NPC_ALDEN`) |
| 1 | male | black, short, heavy fringe | none | none | unused |
| 2 | male | white, short | none | thick dark brows | unused |
| 3 | male | hidden by the hood | white pointed hood | strap or lacing marks | unused |
| 4 | male | black, short, heavy fringe | none | black eyepatch with strap; pin at the collar | unused |
| 5 | male | medium brown, short | white headband | none | unused |
| 6 | male | dark brown, short, ruffled | none | strap or lacing marks | unused |
| 7 | male | bald; black hair ring at the back | none | black moustache; wide dark sash | Bram (`NPC_BRAM`) |
| 8 | female | medium brown, long, centre parting | none | none | Eliza (`NPC_ELIZA`) |
| 9 | female | black, long, centre parting | none | none | unused |
| 10 | female | pale blond, long (assigned) | none | thick dark brows | unused |
| 11 | female | medium brown, long, straight strands | none | smile; collar and hip marks | unused |
| 12 | female | black, long | wide-brimmed hat | none | unused |
| 13 | female | dark brown, long, ruffled | none | collar and hip marks | unused |
| 14 | female | medium brown, long | polka-dot kerchief | dark waist sash | unused |
| 15 | female | silver-gray, long, centre parting | none | no brows | unused |

## Rows

### Row 0: the traveler

Used by the player and Alden's (`NPC_ALDEN`) party slot.

- **Sex:** male.
- **Hair:** medium brown, cropped close around the head, with a straight-cut fringe
  across the forehead.
- **Head covering:** none.
- **Clothing:** plain white tunic, dark belt, dark outline. No markings.
- **Basis:** hatched hair on top and back; solid dark fringe line on pixel row 4
  of the S frame.

### Row 1: black short hair

- **Sex:** male.
- **Hair:** black, short, falling over the forehead in a heavy fringe, with a small
  tip at the centre. The back of the head is solid black with two white glints.
- **Head covering:** none.
- **Clothing:** plain tunic and belt.
- **Basis:** S frame rows 1-4 solid black with white glints.

### Row 2: white short hair

- **Sex:** male.
- **Hair:** white (reads as white or silver), short and smooth with two dark specks.
- **Head covering:** none. The dome is smooth with no brim or hood edge, so it
  reads as hair and not a cap.
- **Face:** thick dark eyebrows, drawn as a broken bar on pixel row 4.
- **Clothing:** plain tunic and belt.
- **Basis:** white head on rows 1-3 of the S frame; dark bar on row 4.

### Row 3: hooded ranger

- **Sex:** male.
- **Hair:** hidden. The hood covers the whole head, so no hair colour is shown.
- **Head covering:** a white hood with a pointed top. From behind it has a centre
  seam. From the front a dark band runs across the brow and the face sits in a
  framed opening. In profile the hood is a cone over the head that overhangs the
  brow, with a dark rim between the cloth and the face and the cloth continuing
  behind the cheek.
- **Clothing:** tunic and belt, with a dark mark at the collar and another at the
  hem, the same from front and back. They read as a strap or tunic lacing.
- **Basis:** S frame rows 0-4 form the pointed hood; N frame has a vertical seam
  on pixel column 7; E frame rows 0-4 form the cone, row 5 is the shadow under it,
  and a rim runs down pixel column 8.

### Row 4: eyepatch

- **Sex:** male.
- **Hair:** black, short, heavy fringe, drawn as in row 1.
- **Head covering:** none.
- **Face:** a black eyepatch over the character's right eye (the viewer's left in
  the S frame), with a strap running diagonally up across the forehead to the hair.
- **Clothing:** plain tunic and belt, plus one dark mark at the collar centre. It
  reads as a pin, button or pendant on the front only.
- **Basis:** patch is a 2 x 2 block at pixels (5-6, 7-8) of the S frame; strap
  steps up from it to the fringe.

### Row 5: headband

- **Sex:** male.
- **Hair:** medium brown, short; hatched on the crown and at the back.
- **Head covering:** a white headband around the head at forehead height, with a
  dark lower edge. It is visible from every direction.
- **Clothing:** plain tunic and belt.
- **Basis:** white band on pixel row 3 and dark edge on row 4, with hatching above
  and below it in the N frame.

### Row 6: dark brown ruffled hair

- **Sex:** male.
- **Hair:** dark brown, short, with a dense ruffled or slightly curly texture and a
  dark band across the brow.
- **Head covering:** none.
- **Clothing:** tunic and belt, with the same collar and hem marks as row 3.
- **Basis:** checker texture over the whole head; dark inset band on row 4.

### Row 7: bald with a moustache

Used by Bram (`NPC_BRAM`).

- **Sex:** male.
- **Hair:** none on top; the crown is a smooth white dome. A ring of black hair
  circles the back of the head at ear level, visible from behind and the sides.
- **Head covering:** none.
- **Face:** a black moustache bar under the eyes.
- **Clothing:** tunic with a double-thickness dark sash at the waist, noticeably
  heavier than the single belt on the other rows.
- **Basis:** white dome on rows 1-4 of the S frame; black band on rows 5-7 of the
  N frame; moustache on row 8.

### Row 8: brown, long, plain dress

Used by Eliza (`NPC_ELIZA`).

- **Sex:** female.
- **Hair:** medium brown, long, falling past the shoulders to mid-back, with a
  fringe split by a centre parting.
- **Head covering:** none.
- **Clothing:** plain white dress with a flared hem. No markings.
- **Basis:** hatched hair continuing below the head in the N and side frames; white
  parting pixel in the fringe line.

### Row 9: black, long, plain dress

- **Sex:** female.
- **Hair:** black, long to mid-back, with bangs and a centre parting.
- **Head covering:** none.
- **Clothing:** plain dress with a flared hem.
- **Basis:** solid black crown and hair continuing below the head.

### Row 10: pale blond, long

- **Sex:** female.
- **Hair:** pale blond or white, long to mid-back and smooth, with a few dark
  specks. **Assigned** blond; the sprite is only white with specks.
- **Head covering:** none (same smooth-dome reading as row 2).
- **Face:** thick dark eyebrows.
- **Clothing:** plain dress with a flared hem.
- **Basis:** white head with dark bar on pixel row 4 of the S frame.

### Row 11: brown, long, straight strands

- **Sex:** female.
- **Hair:** medium brown, long, with a straight fringe and no parting. It is drawn
  as vertical strands where row 8 has diagonal hatching.
- **Head covering:** none.
- **Face:** a small smile, two dark pixels under the face. It is the only row with
  a mouth.
- **Clothing:** dress with a flared hem, with one dark mark at the collar and one at
  the hip. They read as a pin and a pouch or pocket.
- **Basis:** vertical stripes on the crown and back of the head; smile at pixels
  (7-8, 9) of the S frame.

### Row 12: black, long, wide-brimmed hat

- **Sex:** female.
- **Hair:** black, long to mid-back. It is covered on top; the long hair shows below
  the brim from behind and the sides.
- **Head covering:** a wide-brimmed hat. The crown is a white dome with a black band
  at its base, and a white brim flares out past the head on both sides. The brim
  hides the forehead, and the eyes show just below it.
- **Clothing:** plain dress with a flared hem.
- **Basis:** crown on pixel rows 0-3, band on row 4, brim on row 5.

### Row 13: dark brown ruffled, long

- **Sex:** female.
- **Hair:** dark brown, long, with the same ruffled or slightly curly texture as
  row 6.
- **Head covering:** none.
- **Clothing:** dress with a flared hem, with the collar and hip marks as row 11.
- **Basis:** checker texture over the whole head, continuing below it.

### Row 14: brown, long, kerchief

- **Sex:** female.
- **Hair:** medium brown, long. It is covered on top; the hatched hair shows below
  the kerchief from behind and the sides.
- **Head covering:** a white kerchief with scattered black dots, tied with a black
  band across the forehead. It reads the same from every direction.
- **Clothing:** dress with a dark sash at the waist, above where the skirt flares.
- **Basis:** dotted white on pixel rows 1-3, black band on row 4; the sash is one
  extra dark row on the body.

### Row 15: silver-gray, long, parted

- **Sex:** female.
- **Hair:** silver-gray (white with a few sparse dark diagonal strokes), long to
  mid-back, parted down the centre by a dark line over the crown.
- **Head covering:** none.
- **Face:** no eyebrows. A short dark hairline, two pixels, crops in from each side
  of the forehead and leaves the centre open.
- **Clothing:** plain dress with a flared hem.
- **Basis:** parting on pixel column 7; hairline stubs on row 4 of the S frame at
  columns 4-5 and 10-11.
