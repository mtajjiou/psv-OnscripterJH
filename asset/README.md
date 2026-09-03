# Button glyphs

`btn_circle.png`, `btn_cross.png`, `btn_square.png` and `btn_triangle.png`
are the face-button prompts the launcher and the in-game overlay draw their
hints with.

Only the face buttons. `import_button_icons.py` will produce the shoulder,
start, select, d-pad and stick icons too, and they are deliberately not
used: those files are pictures of the physical buttons -- a shoulder seen at
an angle, an oval with SELECT set inside it -- which read at the size a
manual prints them and turn to blobs at the height of a line of text. The
interface draws those as chips of their letters instead, which is legible at
any size and is how the console writes them as well.

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
