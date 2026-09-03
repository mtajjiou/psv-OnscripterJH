# Button glyphs

`btn_circle.png`, `btn_cross.png`, `btn_square.png`, `btn_triangle.png`,
`btn_l.png`, `btn_r.png`, `btn_start.png`, `btn_select.png`, `btn_dpad.png`
and `btn_lstick.png` are the button prompts the launcher and the in-game
overlay draw their hints with.

They were produced by `script/import_button_icons.py` from a set of PS Vita
button icons supplied for this project. That script trims each icon to what
it actually draws, scales it to a common height keeping its own proportions,
and inverts its luminance: the icons are drawn for light backgrounds, with a
dark body and the symbol knocked out of it, and this interface is nearly
black, so used unchanged they would be a dark shape on a dark panel.

To swap in a different set, point the script at the directory:

    python3 script/import_button_icons.py ~/wherever asset

Its `WANTED` table maps each output name to the file it comes from; a set
that names its files differently needs that table adjusted and nothing else.
The interface reads each image's own dimensions, so any resolution works.

`script/make_button_glyphs.py` remains too. It draws the four face-button
shapes from scratch -- white, with the shape in the alpha channel -- and was
what the launcher used before real icons were available. It is still the
fallback for anyone who would rather ship nothing they did not draw.

The icons in this directory carry whatever licence their author placed on
them, and the console makers' button artwork is trademarked. That is a
decision for whoever publishes a build, which is why nothing here is
downloaded automatically.
