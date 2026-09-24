# com.example.shapes

Shapes of the plane and what they measure.

Every length is a whole number of `UNIT`, so a measurement never carries a
fraction. The module holds:

- `Circle`, the one shape with a body
- `Figure`, the variant a drawing stores

```anti
let c = shapes.Circle { radius: 2, stroke: shapes.Stroke.Solid };
let a = shapes.area(c);
```

The rules of the plane stand in [the handbook](https://example.com/plane).

## Stroke

```anti
pub enum Stroke: byte
```

How an outline is drawn.

- `Solid = 0`
  A line with no gaps.
- `Dashed = 1`
  A line with gaps.

## UNIT

```anti
pub const UNIT: int = 1
```

The unit every length is written in.

## Circle

```anti
pub struct Circle
```

A circle, named by its radius.

- `radius: int`
  The radius, in whole units of `UNIT`.
- `stroke: Stroke`
  How the outline is drawn.

## Figure

```anti
pub variant Figure
```

Either of the two figures a drawing stores.

- `Round { radius: int }`
- `Empty`

## Canvas

```anti
pub class Canvas
```

A drawing that counts the shapes put on it.

- `name: str`
  The name of the drawing.

- `pub fn new(name: str) -> *Canvas`
  A canvas with a name.
- `pub fn add(self, shape: Circle)`
  Put `shape` on the canvas.
- `pub fn size(self) -> int`
  How many shapes stand on the canvas.

## area

```anti
pub fn area(c: Circle) -> int
```

The area of `c`, rounded down.

## bounds

```anti
pub fn bounds(c: Circle) -> (int, int)
```

The width and the height of `c`.

## visit

```anti
pub fn visit(list: []Circle, each: fn(Circle), concurrent weigh: fn(Circle) -> int, keep done: fn(int)) -> int
```

Give `each` every circle of `list`, add up what `weigh` says of them and
keep `done` for later.

## parse

```anti
pub fn parse(text: str) -> int may fail
```

The radius `text` names, or a failure when it names none.

## measure

```anti
pub worker fn measure(c: Circle) -> int
```

Measure `c` on a worker.

## Tray

```anti
pub synchronized class Tray
```

Circles that threads add to under the lock of the object.

- `pub fn add(self, c: Circle)`
  Runs under the lock of its object.
  Add one circle.

