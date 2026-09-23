# Repository layout

BLOCKED before the first move. Nothing was moved and no code changed.

## The conflict

The step asks for a rule that the top level holds exactly `build/`, `docs/`, `LICENSES/`,
`src/`, `tests/` and `tools/`, plus `CLAUDE.md`, `README.md`, `CHANGELOG.md`, `LICENSE`,
`CMakeLists.txt`, `CMakePresets.json` and `.gitignore`. It also asks for a test,
`repo_layout`, that fails on a tracked path whose top-level directory is outside that set,
and it says adding to the set is Eddie's decision.

Four tracked entries at the top level fall outside the set:

| Path | What depends on it |
|---|---|
| `.github/workflows/test.yml` | GitHub reads workflows from `.github/` alone. The test `workflows_dispatch_only` reads `${PROJECT_SOURCE_DIR}/.github/workflows`. |
| `.gitattributes` | Checks every file out with line feeds, so expected outputs compare byte for byte. The test `line_endings` depends on it. |
| `.editorconfig` | Editor settings. |
| `r` | The link to `tools/release.sh` that `./r` runs, as `CLAUDE.md` and `docs/decisions.md` describe the release. |

`repo_layout` as specified fails on `.github/`. Moving or deleting any of the four changes
behaviour, which the step forbids. Keeping them means adding to the allowed set, which is
Eddie's decision. The step leaves no option a session may take alone.

## Question for Eddie

Does the allowed top level also hold `.github/`, `.gitattributes`, `.editorconfig` and
`r`? If any of them should go instead, where does it go, and what replaces `./r`?

With the answer the step runs unchanged from the start.
