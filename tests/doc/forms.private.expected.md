# com.example.forms

The forms of an item that the page of `shapes` does not hold, one item each.

A paragraph with an & and a "quote" in it, and with an ` alone.
```anti
let x = 1;
```

- an item of a list
```anti
let y = 2;
```
- an item that goes on over a second line

Names: `com.example.forms`, `forms.Level`, `limits.Sized`, `stop()`,
`com.example.forms.Level`, `forms.none_such`, `other.Thing`, `.dot`, `9nine`
and `a-b`. A bracket [alone and a [bracket](without an end.
- a list that ends the text

## TOP

```anti
pub const TOP: byte = 255
```

The largest byte.

## ON

```anti
pub const ON: bool = true
```

A flag.

## HALF

```anti
pub const HALF: float = 0.5
```

A half.

## NOTHING

```anti
pub const NOTHING: ?*int = none
```

No pointer.

## NAME

```anti
pub const NAME: str
```

A text.

## Same

```anti
pub constraint Same = eq
```

What two values of a type are compared by.

## Level

```anti
pub enum Level: i32
```

A level.

- `Low = 0`
- `High = 1`

## Tight

```anti
pub packed struct Tight
```

A struct without padding.

- `a: byte`
- `b: u32`

## Wide

```anti
pub align(16) struct Wide
```

A struct on a boundary of sixteen bytes.

- `a: byte`

## Lanes

```anti
pub simd struct Lanes
```

Four lanes.

- `a: f32`
- `b: f32`
- `c: f32`
- `d: f32`

## Word

```anti
pub union Word
```

A word read two ways.

- `whole: u32`
- `real: f32`

## Named

```anti
pub abstract class Named
```

What a named thing gives.

- `pub abstract fn name(self) -> str`
  The name.

## Part

```anti
pub class Part
```

A part another class holds.

- `pub n: int`
  A count.

## Base

```anti
pub class Base
```

A base.

- `pub n: int`
  A number.

- `pub fn get(self) -> int`
  The number.

## Leaf

```anti
pub final class Leaf inherits com.example.forms.Base
```

A class with every kind of field.

- `implements named: Named`
- `use part: Part`
  A part whose functions the class takes.
- `pub own next: ?*Part`
  An owned pointer.
- `pub low: byte : 3`
  Three bits.

- `pub concrete fn name(self) -> str`

## Watched

```anti
pub trace class Watched
```

A class whose calls are traced.

- `pub n: int`
  A number.

## Locked

```anti
pub synchronized class Locked
```

Holds a number under the lock of the object.

- `n: int`
- `(lock): Object lock`

- `pub fn get(self) -> int`
  Runs under the lock of its object.
  The number.

## Counted

```anti
pub concurrent class Counted
```

A class of atomic fields.

- `n: int`

## printf

```anti
pub extern fn printf(format: ?*byte, ...) -> i32
```

A function of C with any number of arguments.

## stop

```anti
pub fn stop() -> never
```

Ends the program.

## same

```anti
pub fn same<A: Same + limits.Sized, B>(a: A, b: B) -> bool
```

Whether a value of one type equals itself, beside one of another.

## hold

```anti
pub fn hold(keep own f: fn(int) -> int, keep g: fn(int) -> int)
```

Keeps the two functions for a later call.

## eq

```anti
pub fn eq(a: Tight, b: Tight) -> bool
```

Two levels are equal.

