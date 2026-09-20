# The signing key has one path, and git may not see it

Eddie decided on 2026-09-21 that `RELEASE_KEY` goes and that the key stands at
`keys/private/release-key.pem`. The rule to enforce is not where the key sits.
It is that git cannot see it. The key is plaintext, because it lives on
an offline Mac mini that cuts every release, and that machine is the protection.
This session made the change and ran `./r --dry-run`. Nothing was signed, tagged
or uploaded.

## What changed

- `./r` reads `keys/private/release-key.pem` and names no environment variable.
  Without the file it prints the two signing commands and stops before the tag.
  That is what every machine but the one that holds the key does.
- `check_key` refuses the key that `git ls-files` calls tracked, and the key
  that `git check-ignore` does not call ignored. Either refusal names the file.
  Both run in the preflight as well, so a release stops before the suites rather
  than after them.
- An encrypted PKCS#8 key still signs, with its passphrase asked once. A
  plaintext key signs with no prompt. A file that holds no private key stops the
  run.
- The rule that the key must live outside the tree is gone, with the resolution
  of paths that went with it. It answered the wrong question. The earlier report
  found that a plaintext key signed silently. The decision above answers it, and
  no refusal does.

## The order of the checks

`git check-ignore` consults the index, so a file that is both tracked and
excluded answers "not ignored". The tracked check therefore runs first, and its
message is the one a reader gets. `--no-index` would report the rule alone, and
would say nothing about the danger.

## The test

`release_dry_run` gained six cases. A plaintext key at the path. An encrypted
key at the path. A key git tracks. A key no rule of `.gitignore` excludes. A
file that holds no private key, and the run with no key at all. The two refusals
commit their change first, because the preflight refuses a dirty index before it
reads the key.

Each guard was removed in turn, and the test failed each time. The in-tree rule
of the earlier draft was proven the same way before it was replaced.

## What the change found

`key_path` already named the path of the public key under the webroot, which
step 9 rsyncs. The new variable of the same name overwrote it, and the first run
of the test refused `keys/release.pem` as a tracked signing key. The site's
variable is `site_key_path` now. A shell has one scope, and a name that reads
well twice is a name that collides.

## The state of the key

`keys/private/` holds `release-key.enc.pem` and no `release-key.pem`. The dry
run therefore says the key is missing and that a run would stop before the tag.
The plaintext key of the Mac mini goes at
`keys/private/release-key.pem` before the real release. `.gitignore` excludes
`keys/private`, and `git check-ignore -v keys/private/release-key.pem` names
that rule, so the guard passes once the file is there. Nothing of the key was
moved, decrypted or read by this session.

## The dry run

`ANTI_SITE=triton.niese.net:/var/www/anti-lang.com/webroot ./r --dry-run` on
`d7f65b3`.

| Step | Outcome |
|---|---|
| 1 | The key is missing, so the run would stop before the tag |
| 2 | Mac 494, ASan 493, UBSan 493, in an export of the commit |
| 3 | Six packages, levels checked, macos-x86_64 under Rosetta |
| 5 | anti-linux 431 tests, anti-windows 411 tests, each installs and uninstalls |
| 7 | Would upload 13 files, and no signature |
| 9 | Would rsync the two installers, the page, `SHA256SUMS.sig` and the key |
