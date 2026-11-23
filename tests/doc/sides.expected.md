# com.example.sides

The forms of an item that the pages of `forms` and `shapes` do not hold.

## Count

```anti
pub type Count = int
```

A count of things.

## check

```anti
pub fn check(n: int) may fail
```

Checks `n`, and fails for one below zero.

## call_back

```anti
pub extern fn call_back(f: fn(c_int) -> c_int) -> c_int
```

Calls `f` from C.

## THIRD

```anti
pub const THIRD: f64 = 0.30000000000000004
```

The sum of a tenth and a fifth, which takes seventeen digits.

## TENTH

```anti
pub const TENTH: f32 = 0.1
```

A tenth at the width of an `f32`.

## TWO

```anti
pub const TWO: f64 = 2.0
```

A whole number of type `f64`.

## ENDLESS

```anti
pub const ENDLESS: f64
```

A value without end, which no literal writes.

## Mine

```anti
pub class Mine inherits anti.lang.Error
```

An error of the module.

## Store

```anti
pub synchronized class Store
```

A store under the lock of the object.

- `pub fn construct(self)`
  Makes the store.
- `pub fn destruct(self)`
  Ends the store.
- `pub fn describe(self) -> int`
  Runs under the lock of its object.
  Describes the store.
- `pub fn zero() -> int`
  Makes a count without a store.

