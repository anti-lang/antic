# anti.graphics

The design of the module. The decision of 2026-10-28 under "Feature freeze
of 2026-10-14" sent it to Eddie before it is built, and he approved it for
building on 2026-11-06, under "Answers of 2026-11-06" in `docs/decisions.md`.
"Standard interfaces" in `docs/anti-language-additions.md` holds its rules.
`anti.math`, which the module uses, is designed in `docs/notes/math.md`.

## Intent

A program draws with `import anti.graphics;` and nothing else. It names no
pixel size, colour depth or pixel format and needs no manifest entry. The same
program looks the same on a laptop, a 4K monitor, a portrait screen and a
display without HiDPI, on all six targets.

Eddie's rule for the design: stay close to what raylib offers. Every part
below names the raylib function or concept it stands on. A program that knows
raylib reads it at once, and one that needs more imports `anti.raylib`.

"Standard interfaces" decides three things already. The module is Anti
classes over raylib. No raylib type stands in a `pub` signature. The backend
can be replaced through `[inject]`.

## The first program

```anti
import anti.graphics.{xy};

fn main() -> int
{
	let w = graphics.open("Hello") catch e {
		e.fatal();
	};
	for f in w.frames() {
		f.clear(graphics.RAYWHITE);
		f.circle(xy(0.5, 0.5), 0.25, graphics.PINK);
	}
	return 0;
}
```

- `graphics.open` is `SetConfigFlags` and `InitWindow`. It may fail with
  `graphics.NoDisplay` on a machine without one.
- `for f in w.frames()` is raylib's loop. Each turn is one `BeginDrawing` and
  `EndDrawing`, and the loop ends when `WindowShouldClose` says so. State that
  lives across frames is an ordinary local of `main`.
- The end of `w` is `CloseWindow`.

## The window

`open(title, window: Window = Window {})` takes a `Window`, a class whose
fields are the config flags of raylib, each with a default, so a literal
names only what it changes: `Window { vsync: false }`. The defaults:

- two thirds of the monitor, from `GetMonitorWidth` and `GetMonitorHeight`;
- `FLAG_WINDOW_RESIZABLE`, `FLAG_WINDOW_HIGHDPI`, `FLAG_VSYNC_HINT` and
  `FLAG_MSAA_4X_HINT` on;
- Escape closes, as raylib's default exit key.

The window object carries the rest of raylib's window and timing functions
under their own names without the prefix: `w.time()` is `GetTime`,
`w.frame_time()` is `GetFrameTime`, `w.toggle_borderless()` is
`ToggleBorderlessWindowed`, `w.set_target_fps(n)` is `SetTargetFPS`.

## Coordinates: the unit square

Every position and size is in units of the square. The square runs from 0.0
to 1.0 on both axes. It is the largest square that fits the window. It stands
in the centre and is always fully visible. raylib does this already through
`Camera2D`. Its offset centres the square. Its zoom is the number of pixels
in one unit, the shorter side of the render size.

- y points down and the origin is the top left of the square, as in raylib.
  A rotation is in degrees and turns clockwise, as in `DrawRectanglePro`.
- A circle is round on every window, because one unit is the same length on
  both axes.
- A window wider than high shows more than the square at the left and the
  right, a portrait window shows more above and below. A 16:9 window shows x
  from -0.389 to 1.389. `f.bounds` gives the visible rectangle, for a
  background or an element anchored to an edge.
- `f.pixel` is the length of one physical pixel in units, for a line of
  exactly one pixel. Text sizes are in units too, so text scales with the
  window.

The pixel size of the window, the HiDPI factor of the monitor and a resize
change none of the numbers a program writes.

## Colour

`Color` is raylib's: four `u8`, red, green, blue and alpha, in sRGB. The
program never sees a pixel format or a colour depth. raylib converts to what
the display takes.

- raylib's named colours are constants of the module: `PINK`, `LIME`,
  `RAYWHITE` and the rest.
- `Color.hex(0xFF1493FF)` is `GetColor`, `Color.hsv(h, s, v)` is
  `ColorFromHSV`, `c.fade(0.3)` is `Fade`, `a.lerp(b, t)` is `ColorLerp`
  and `c.brightness(k)` is `ColorBrightness`.

## Shapes

Each is a `Draw` function of raylib under its name without the prefix, with
its positions and sizes in units: `clear`, `rect`, `rect_lines`,
`rect_gradient_v`, `circle`, `circle_gradient`, `circle_lines`, `ellipse`,
`line` with a thickness as `DrawLineEx`, `triangle`, `poly`, `text` and
`texture`. raylib's counter-clockwise rule for triangles on the screen
holds, so a triangle of the program fills whatever order its corners take.

## Transforms

`f.move(x, y)`, `f.turn(degrees)` and `f.scale(k)` return a new canvas whose
unit square is the old one moved, turned or scaled. They stand on the matrix
of rlgl, `rlTranslatef`, `rlRotatef` and `rlScalef`, which raylib applies to
every vertex as it batches. A canvas is a small value, so recursion gives
each call its own frame of reference, and nothing is pushed by hand or
allocated. `square(color)` fills the unit square of a canvas. The Pythagoras
tree is then:

```anti
fn branch(c: graphics.Canvas, level: int, a: f32)
{
	c.square(PINK.lerp(LIME, level as f32 / LEVELS as f32));
	if level == LEVELS { return; }
	let r = a * math.DEG2RAD;
	let left = c.turn(-a).scale(math.cos(r)).move(0.0, -1.0);
	branch(left, level + 1, a);
	branch(left.move(1.0, 1.0).turn(90.0).scale(math.tan(r)).move(0.0, -1.0),
		level + 1, a);
}
```

A canvas's square stands on its bottom edge. The left child stands on the
first part of the top edge of its parent, turned up by `a` degrees. The right
child stands on the rest, turned a quarter further.
The builder checks the geometry of this sketch with a test of the Recorder
below.

## Text, fonts and images

- `f.text(at, size, s, color)` is `DrawTextEx` with raylib's default font, and
  `Font.load(path, size)` is `LoadFontEx` for a TTF the program ships.
- A frame of a game shows a score without allocating. raylib's `TextFormat`
  writes into a static buffer it reuses, and the window owns an
  `anti.text.Builder` the same way, cleared at each frame:
  `f.text(at, size, w.say().add("Score ").add_int(score), color)`.
- `Texture.load(path) may fail` is `LoadTexture`, and `f.texture(t, at,
  width, tint)` is `DrawTexturePro`.

## Input

raylib's functions under their names without the prefix, positions in units
of the square: `w.key_pressed(Key.F)`, `w.key_down`, `w.key_released`,
`w.mouse()`, `w.mouse_pressed(Mouse.LEFT)`, `w.mouse_wheel()`, `w.touch(i)`
and `w.gamepad_button_down(0, Pad.A)`. `Key`, `Mouse` and `Pad` are enums of
the module with raylib's values.

## Testing and the backend

`anti.graphics.Display` is the abstract class behind the window and the
canvas, with `RaylibDisplay` as its default provider. `graphics.Recorder` is
a second provider that opens no window and records every call. In
`[inject.test]`, `anti test` then runs the drawing code of a program without
a display:

```anti
tests {
	test "the tree draws every square" {
		let r = graphics.Recorder.new(16.0 / 9.0);
		branch(r.canvas(), 0, BASE_ANGLE);
		assert r.count(graphics.Shape.Square) == SQUARES;
	}
}
```

A release build with one provider devirtualises the calls, so the
indirection costs nothing in the frame.

## Rules the module keeps

- No allocation in a frame. `frames`, `Canvas`, `say` and every shape
  allocate nothing after the window opens.
- `open` is called from the main thread, which macOS requires, and refuses
  any other thread.
- All six targets build every example of this page, and the Recorder tests
  run on every host.
- 3D, cameras, shaders, render textures and audio are not part of it. A
  program imports `anti.raylib` for them, and `anti.audio` gets a draft of
  its own over miniaudio.
