# Button glyphs

`btn_circle.png`, `btn_cross.png`, `btn_square.png` and `btn_triangle.png`
are the face-button shapes the launcher and the in-game overlay draw their
hints with. They were produced by `script/make_button_glyphs.py` -- white,
with the shape in the alpha channel, so the interface can tint them.

Replacing them is a matter of dropping files in with the same names: they
are scaled from their own dimensions, so any resolution works, and the
alpha channel is what is drawn, so a white-on-transparent image behaves
best.

Four more are optional and absent by default:

    btn_l.png  btn_r.png  btn_start.png  btn_select.png

Those buttons have no shape of their own, so the interface writes their
names -- as the console itself does. Put images here under those names,
add them to VITA_PACK_ARGS in the top-level CMakeLists.txt, and they are
used instead, with no code change.

Anything taken from elsewhere carries whatever licence its author put on
it, and the console makers' own button artwork is trademarked; that is a
decision for whoever ships the build, which is why nothing here is
downloaded automatically.
