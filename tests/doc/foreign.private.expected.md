# com.example.foreign

A function whose parameter has a type of another module.

## note

```anti
pub fn note(to: *log.Logger)
```

Write nothing to `to`.

## Held

```anti
pub struct Held
```

Hold an allocator, a list of loggers and a clock.

- `from: ?*m.Allocator`
- `loggers: ?*list.List<*log.Logger>`
- `clock: ?*Clock`

## Keeper

```anti
pub class Keeper
```

Take a logger and give back its allocator.

- `pub n: int`

- `pub fn with(self, to: *log.Logger, from: *m.Allocator) -> *m.Allocator`
  The allocator of `to`.

