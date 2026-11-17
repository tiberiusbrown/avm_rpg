# NPC face authoring guide

This guide tells agents and artists how to draw the dialogue portraits in `faces32.png`. It
collects the project owner's direction for faces, with the measurements that came out of it.
Read [actors16_descriptions.md](actors16_descriptions.md) first: it says what each face must
match in its NPC's sprite.

## Files

- `assets/images/faces32.png`: the portrait sheet, described below.
- `assets/images/actors16.png`: the overworld sprites. A face follows its NPC's sprite.
- `assets/images/actors16_descriptions.md`: the sex, hair and head covering of every person
  row. It is the reference a face is drawn from.
- `src/npc_ids.h`: `NPC_ID` and `NPC_ID_LIMIT`. They set the number of rows in the sheet.

## Status

The generator reads the sheet into `FACE_SPRITES` and `FACE_PRESENT` in `generated/sprites.cpp`,
and `src/dialogue_overlay.cpp` draws the face of the NPC that speaks. A row that is all black has
no portrait, and its speaker keeps the plain name panel. The build fails when the sheet's row
count differs from `NPC_ID_LIMIT`. No generator script for the faces themselves is checked in;
edit the PNG directly.

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
| Columns | 2: column 0 has the mouth closed, column 1 has it open |
| Rows | One per `NPC_ID`, so row `n` is the face of NPC ID `n` |
| Size | 64x128 for the four current IDs (`NPC_ID_LIMIT` is 4) |
| Pixels | Opaque black or opaque white only |
| Background | Opaque black everywhere. Never transparent. |
| Row 0 | `NPC_NONE`, the unnamed traveler. It has no face, so both frames are solid black. |

A row for an NPC with no portrait yet is solid black. To add an NPC, append one 32-pixel row
when `NPC_ID_LIMIT` grows, so the row count and the limit stay equal.

## Whose face it is

- **A face belongs to an NPC ID, not to a sprite row.** Two NPCs that share a sprite row can
  have different faces.
- **Match the sprite.** The face must agree with the NPC's sprite in sex, hair colour and
  style, and head covering, as written in `actors16_descriptions.md`. Hair hidden by a hood
  stays hidden.
- **Draw an original face. Never scale up the sprite's head.** The face is a complete redraw
  in the style below.
- **When a sprite changes, change the description and the face with it.** Moving Iona to row 9
  is an example: her face follows the new row.

| NPC ID | Name | Sprite row | What the face shows |
| --- | --- | --- | --- |
| 1 `NPC_VILLAGER` | Iona | 9 | Female. Long black hair, bangs and a centre parting. No head covering. |
| 2 `NPC_WANDERER` | Silas | 2 | Male. Short white hair and thick dark brows. No head covering. |
| 3 `NPC_ROWAN` | Rowan | 3 | Male. A white hood hides his hair. A strap crosses his chest. |

## Art style

The style comes from a 32x32 portrait sheet the project owner supplied as the target. It is
not stored in this repository. Emulate it:

- **Bold, simple linework.** A head-and-shoulders bust fills the frame, and the shoulders run
  off the bottom edge.
- **The edge has three layers.** A white fill (skin or cloth) has a black line around it and a
  thin white line outside the black one. The reference does this on its white shapes, so match
  it: it is what keeps a figure legible against the dark panel.
- **Dark masses are black inside a white line.** Hair is black with a thin white line around
  it, and white highlight strokes inside it: a parting, sheen and strand lines.
- **Shapes are natural, not mechanical.** A hood is a rounded, soft mass whose tip droops and
  curls, with curved fold lines in the cloth and a draped cowl on the shoulders. A straight-edged
  triangle looks like a tin man and is wrong. Hair falls and flares the same way.
- **Features are small and plain.** Eyes are short dashes with a white catch-light, brows are
  strokes above them, the nose is a few pixels, and the mouth is a short line.
- **Put a visible hairline between white hair and white skin.** When hair and face are the same
  colour, a dark line has to separate them or they merge. Silas has an arc across his forehead.
- **Judge the face on black.** The panel is black, so black at the edge of a figure is
  invisible. A feature has to show as a white shape, or as black inside a white shape.

## Uniform feature spacing

Characters share one grid for their features. Variation belongs to the character's own marks:
brows, nose length, lashes and wrinkles. It does not belong to where the eyes and mouth sit.

| Feature | Position |
| --- | --- |
| Eyes | Three pixels wide, two tall. Rows 15-16, at columns 11-13 and 18-20. The top-left pixel is white as a catch-light. |
| Brows | Rows 12-13 over the eyes. Iona's are a thin arc and Silas's are thick. Rowan's are lost in the shadow under his hood. |
| Nose | Ends on row 18 for everyone. |
| Closed mouth | A line on row 21, at columns 14-17. |
| Face size | Iona's face and Rowan's hood opening are both 16 pixels wide and 14 rows tall, from the forehead shadow to the chin. Silas's broader head is 20 wide, with the same features inside it. |

The face does not shrink to fit its surroundings. Rowan's hood opening was enlarged to match
Iona's face for this reason.

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

- The sheet is 32 pixels wide per column and 32 tall per NPC ID, in two columns, with
  only opaque black and white.
- Frame 0 is solid black.
- In every face the eyes, nose and mouth are on the shared grid, and the closed and open mouths
  have the same width.
- Compare the closed and open frames: only the mouth and the rows below it should differ, and
  they differ by the two-row jaw drop.
- View the sheet on black at true size, and again enlarged. The face must read at true size.
- The face still agrees with `actors16_descriptions.md`.
- `docs/tiled_pipeline.md` describes the sheet. Keep its "NPC faces" note in step with this
  guide.
