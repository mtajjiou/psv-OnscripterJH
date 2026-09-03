# Button glyphs

The ten `btn_*.png` files are the button prompts the launcher and the
in-game overlay draw their hints with: the four face buttons, both
shoulders, start, select, the d-pad and the left stick.

Two things about them are worth knowing before regenerating or replacing
them.

They are exported at 40 pixels tall, which is close to the 15-22 the
interface asks for. Storing them much larger looks like it should be
better and is not: the console would be throwing away most of every icon
to draw it, and the result is the chewed edges these had in their first
build.

They are inverted per icon, not as a set. Most are drawn dark-bodied for
light backgrounds and have to be flipped for this interface; the shoulder
buttons in this set are already light and must be left alone. Inverting
everything is why the bumpers came out dark the first time. The script
measures each one.

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
