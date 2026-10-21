# com.example.operators

Two operators that share the name `eq`, so each stands under its name and
the type of its first parameter.

## Level

```anti
pub struct Level
```

A level.

- `n: int`

## Width

```anti
pub struct Width
```

A width.

- `n: int`

## eq:Level

```anti
pub fn eq(a: Level, b: Level) -> bool
```

Two levels are equal.

## eq:Width

```anti
pub fn eq(a: Width, b: Width) -> bool
```

Two widths are equal.

