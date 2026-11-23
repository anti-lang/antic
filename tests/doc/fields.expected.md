# com.example.fields

The marks of the fields of a class.

## Filled

```anti
pub concurrent class Filled
```

A class whose fields a provider fills, and one that holds a count.

- `inject log: *log.Logger`
  The logger a provider gives.
- `inject final clock: *time.Clock`
  The clock the run-time configuration may not replace.
- `atomic count: int`
  A count that more than one thread adds to.

## Settings

```anti
pub singleton class Settings
```

The one instance of the settings.

- `mutable level: int`
  A field that a function of the module may write.

- `pub fn get() -> *Settings`

