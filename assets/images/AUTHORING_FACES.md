# NPC face authoring guide

This guide tells agents and artists how to draw the dialogue portraits in `faces32.png`. It
collects the project owner's direction for faces, with the measurements that came out of it.
Read [actors16_descriptions.md](actors16_descriptions.md) first: it says what each face must
match in its NPC's sprite.

## Files

- `assets/images/faces32.png`: the portrait sheet, described below.
- `assets/images/faces32.json`: dialogue portrait assignments in NPC ID order.
- `assets/images/actors16.png`: the overworld sprites. A face follows its NPC's sprite.
- `assets/images/actors16_descriptions.md`: the sex, hair and head covering of every person
  row. It is the reference a face is drawn from.
- `src/npc_ids.h`: `NPC_ID` sets the identity associated with each portrait row.

## Status

The generator reads the sheet into `FACE_SPRITES` and `FACE_PRESENT` in `generated/sprites.cpp`,
and `src/dialogue_overlay.cpp` draws the face of the NPC that speaks. Pair 0 holds the original
dialogue portraits beside their variants in pairs 1–3. To assign a portrait to a speaker,
put its zero-based `[row, pair]` in `faces32.json` at the speaker's NPC ID. A `null` entry,
an all-black pair, or an ID beyond the assignment list uses the plain name panel.

## In the dialogue

The face is a tab above the left end of the dialogue panel, with the name panel beside it. The
panel is 31 pixels tall with four lines of text, and the text keeps the full width, so long and
multi-page text paginates as before. The frame is drawn at the top of the screen, so all 32 rows
show, and one black row separates it from the panel. While the speaker types, the mouth
alternates between the two columns every five letters, and it is closed whenever the panel waits
for input or eases in or out.

## Sheet format

| Property | Value |
| --- | --- |
| Frame size | 32x32 |
| Columns | Four pairs of 32-pixel columns. The left frame in each pair has the mouth closed and the right frame has it open. |
| Rows | Row `n` matches actor sprite row `n` across all four pairs. |
| Size | 256x512 |
| Pixels | Opaque black or opaque white only |
| Background | Opaque black everywhere. Never transparent. |

Use `null` for `NPC_NONE` and for any unused NPC IDs between assignments.
Fill any unused slots on the sheet with opaque black.

## Dialogue assignments

- **Assign dialogue portraits by NPC ID.** Two NPCs that share a sprite row can
  have different faces.
- **Match the sprite.** The face must agree with the NPC's sprite in sex, hair colour and
  style, and head covering, as written in `actors16_descriptions.md`. Hair hidden by a hood
  stays hidden.
- **Draw an original face. Never scale up the sprite's head.** The face is a complete redraw
  in the style below.
- **When a sprite changes, change the description and the face with it.** Moving Tessa to row 9
  is an example: her face follows the new row.

| NPC ID | Name | Row in pair 0 | What the face shows |
| --- | --- | --- | --- |
| 1 `NPC_TESSA` | Tessa | 9 | Female. Long black hair, bangs and a centre parting. No head covering. |
| 2 `NPC_ORRIN` | Orrin | 2 | Male. Short white hair and thick dark brows. No head covering. |
| 3 `NPC_ROWAN` | Rowan | 3 | Male. A white hood hides his hair. A strap crosses his chest. |
| 4 `NPC_MARA` | Mara | 11 | Long medium-brown hair with a straight fringe and a collar pin. |
| 5 `NPC_BRAM` | Bram | 7 | Bald crown, black hair at the sides, and a black moustache. |
| 6 `NPC_NELL` | Nell | 12 | Long black hair under a wide-brimmed white hat with a dark band. |
| 7 `NPC_ADA` | Ada | 14 | Long medium-brown hair beneath a white polka-dot kerchief. |
| 8 `NPC_SURVEYOR` | Surveyor | 10 | Long pale-blond hair, thick dark eyebrows, and no head covering. |

## Portrait library

Coordinates start at zero. Each row below identifies a row in both
[`faces32.png`](faces32.png) and [`actors16.png`](actors16.png). Pair 1 uses
frame columns 2–3, pair 2 uses columns 4–5, and pair 3 uses columns 6–7.
The [actor descriptions](actors16_descriptions.md) record the shared sprite traits.

| Actor and portrait row | Pair 1 | Pair 2 | Pair 3 |
| --- | --- | --- | --- |
| 0 | Hatched side sweep, round cheeks | Cropped fringe, angular face, tied collar | Blunt fringe, square face, large ears |
| 1 | Straight black fringe, raised collar | Swept black fringe, cheek crease | Shaggy black fringe, oval face |
| 2 | White hair, heavy brows, deep cheek folds | Swept white hair, narrow jaw | Peaked white hair, long face |
| 3 | Curled white hood, crossed laces | Left-drooping white hood, diagonal shoulder strap | Tall white hood, broad face |
| 4 | Eyepatch and square jaw | Eyepatch and round cheeks | Eyepatch and lined cheeks |
| 5 | Broad headband, sleeveless collar | Narrow headband tied on the right, long face | Headband tied on the left, pointed chin |
| 6 | Short scalloped curls, long face, shoulder strap | Dense short curls, wide brow, top laces | Cropped curls, prominent ears, cheek creases |
| 7 | Bald with a wide handlebar moustache and curved collar | Bald with a compact straight moustache and broad sash | Bald with a drooping moustache and broad chin |
| 8 | Long center-parted hatched hair, narrow face | Long hatched hair tucked behind an ear, rounded face | Long center-parted waves, square neckline |
| 9 | Black curtain bangs, angular cheeks | Symmetric black bangs, pointed collar | Asymmetric black bangs, narrow chin |
| 10 | Long pale-blond waves, wide jaw | Long pale-blond curls, narrow chin | Long pale-blond side sweep, folded neckline |
| 11 | Long striped fringe, round cheeks | Long straight fringe, square collar pin, cheek creases | Short fringe over long straight hair, full cheeks |
| 12 | Wide floppy hat, long face | Round high-crowned hat, small face | Tilted wide-brimmed hat, angular face |
| 13 | Long tight waves, square face | Long corkscrew curls, tapered chin | Broad uneven waves, lined cheeks |
| 14 | Polka-dot kerchief tied on the right, round chin | Snug polka-dot kerchief, cheek creases | Left-tied polka-dot kerchief, long face |
| 15 | Long silver waves, long cheeks, folded collar | Long symmetric silver waves, round neckline | Long center-parted silver hair, angular jaw |

## Art style

The style comes from a 32x32 portrait sheet the project owner supplied as the target. It is
not stored in this repository. Emulate it:

- **Bold, simple linework.** A head-and-shoulders bust fills the frame, and the shoulders run
  off the bottom edge.
- **The edge has three layers.** A white fill (skin or cloth) has a black line around it and a
  thin white line outside the black one. The reference does this on its white shapes, so match
  it: it is what keeps a figure legible against the dark panel. Leave the contour open where
  the shoulders meet the bottom of the frame.
- **Dark masses are black inside a white line.** Hair is black with a thin white line around
  it, and white highlight strokes inside it: a parting, sheen and strand lines.
- **Keep fills clean and patterns deliberate.** Use a few connected strokes for hair
  texture and place kerchief dots at readable intervals. Keep skin and plain cloth
  in solid shapes at 32x32.
- **Shapes are natural, not mechanical.** A hood is a rounded, soft mass whose tip droops and
  curls, with curved fold lines in the cloth and a draped cowl on the shoulders. A straight-edged
  triangle looks like a tin man and is wrong. Hair falls and flares the same way.
- **Shape the crown and temples together.** Round the skull above the ears, with a gradual
  curve into the temples. Give tall hair and hood tips a black margin above them.
- **Show where short hair ends.** Keep it above or behind the ears and expose the cheeks
  below. A cheek contour that turns inward under the ear separates the head from the shoulders.
- **Features are small and plain.** Eyes are short dashes with a white catch-light, brows are
  strokes above them, and the nose is a few pixels. Keep the mouth corners level for a neutral
  expression.
- **Put a visible hairline between white hair and white skin.** When hair and face are the same
  colour, a dark line has to separate them or they merge. Orrin has an arc across his forehead.
- **Draw the chin above the neck.** Carry the black jawline from cheek to cheek and keep the
  skin below it separate in both mouth frames.
- **Judge the face on black.** The panel is black, so black at the edge of a figure is
  invisible. A feature has to show as a white shape, or as black inside a white shape.

## Uniform feature spacing

Characters share one grid for their features. Variation belongs to the character's own marks:
brows, nose length, lashes and wrinkles. It does not belong to where the eyes and mouth sit.

| Feature | Position |
| --- | --- |
| Eyes | Three pixels wide, two tall. Rows 15-16, at columns 11-13 and 18-20. The top-left pixel is white as a catch-light. |
| Brows | Rows 12-13 over the eyes. Tessa's are a thin arc and Orrin's are thick. Rowan's are lost in the shadow under his hood. |
| Nose | Ends on row 18 for everyone. |
| Closed mouth | A straight line on row 21, at columns 14-17. |
| Face size | Tessa's face and Rowan's hood opening are both 16 pixels wide and 14 rows tall, from the forehead shadow to the chin. Orrin's broader head is 20 wide, with the same features inside it. |

The face does not shrink to fit its surroundings. Rowan's hood opening was enlarged to match
Tessa's face for this reason.

## Closed and open mouths

Column 1 is what the player sees while an NPC speaks, alternating with column 0.

- **The mouth does not get wider when it opens.** In the reference the opening grows taller.
  The open mouth has the closed mouth's width: four pixels.
- **The opening is three rows tall and rounded at the bottom.** The first two rows are four
  pixels wide and the last is the middle two.
- **The jaw drops.** Every row of the face below the mouth moves down by two rows, so the chin,
  the jaw line and the lower part of a hood opening all move with it. The face and the hood
  opening get two rows taller.
- **Nothing above the mouth changes** between the two columns: the eyes, brows, hair and the
  top of the hood stay put.

On the grid above: the closed mouth is row 21. The open mouth fills rows 21 and 22 at columns
14-17 and row 23 at columns 15-16.

## Making a face

1. Draw the bust's shapes: white skin and cloth, black hair and shadow.
2. Draw the features on the shared grid, then the character's own marks.
3. Add the black outline around every white shape, then the thin white line outside the black
   one. Hair and other black masses take just the white line.
4. Make the open frame from the closed one: shift the rows below the mouth down by two, then
   replace the mouth with the open shape.
5. Fill every pixel that is not part of the figure with opaque black.

## Checks

- The sheet uses 32x32 frames in complete closed/open column pairs with only opaque
  black and white.
- The first assignment in `faces32.json` is `null` for `NPC_NONE`.
- In every face the eyes, nose and mouth are on the shared grid, and the closed and open mouths
  have the same width.
- Compare the closed and open frames: only the mouth and the rows below it should differ, and
  they differ by the two-row jaw drop.
- View the sheet on black at true size, and again enlarged. The face must read at true size.
- The face still agrees with `actors16_descriptions.md`.
- `docs/tiled_pipeline.md` describes the sheet. Keep its "NPC faces" note in step with this
  guide.
