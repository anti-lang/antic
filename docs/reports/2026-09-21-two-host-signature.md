# The signature leaves the host of the binaries

Eddie decided on 2026-09-21 that the manifest and its signature stand on two
hosts. The GitHub release holds the packages, the symbols archives and
`SHA256SUMS`. anti-lang.com holds `SHA256SUMS.sig` and the public key, beside
the two installers and the downloads page. A forged release then needs both
hosts, rather than the private key alone. This session made the change,
verified the key setup and ran `./r --dry-run`. Nothing was signed, tagged or
uploaded.

## What changed

- Step 7 uploads thirteen files. `SHA256SUMS.sig` is no longer among them.
- Step 9 rsyncs four files: the two installers, the downloads page,
  `SHA256SUMS.sig` into `downloads/anti/<version>/` and `keys/release.pem`. It
  makes the directory of the version over ssh first, because openrsync has no
  `--mkpath`. It then reads the signature and the key back over HTTPS and checks
  that the signature of the site covers the manifest of the release.
- Both installers read the package and `SHA256SUMS` from the release, and
  `SHA256SUMS.sig` from `ANTI_SITE_BASE`, which defaults to anti-lang.com. The
  key stays the one each carries. A staging area keeps its own signature,
  because step 5 installs before step 9 publishes anything.
- `tools/site-base` names the site, the signature path and the key path. Both
  installers carry those addresses, and `installer_github` pins their copies.
- The downloads page links the signature and names the fingerprint of the key.

## The test

`installer_github` stands a fake release and a fake site on disk. It gained three
checks. The signature comes from the site. The log names the site it came from. A
signature that stands on the release beside the binaries is refused, even when it
is the right signature over the right manifest.

`release_dry_run` checks that step 7 plans thirteen files and says it uploads no
signature. The new checks were proven to bite: an installer patched to read the
signature from the release area fails the test.

## The key setup

1. Four copies of the public key are byte-identical, 178 bytes each, SHA-256
   `7e83692f5546e5d1ebaba9fcf074c73c5257ab9f01eb701758c463a96f7a0275`: the one
   `tools/install.sh` carries, the one `tools/install.ps1` carries,
   `keys/release.pem` of the repository and the one
   `https://anti-lang.com/keys/release.pem` serves. openssl reads the same DER
   key from all four, fingerprint
   `7e64c56e26a42946823a66aa1f30bf686b6b5dbd0dc0e2c165a080540ffc3eca`. That key
   also verifies `SHA256SUMS.sig` of `23.1.1-anti.3` of `anti-lang/llvm-tools`,
   so antic and the LLVM tools share one release key.
2. Both installers verify against the key in their own text. Each writes it to a
   file of the work directory and passes that to `openssl pkeyutl -verify`.
   Neither fetches a key. A user who pipes the script into a shell therefore
   trusts the bytes that arrived, and nothing fetched afterwards.
3. `keys/private/release-key.enc.pem` is a PKCS#8 `ENCRYPTED PRIVATE KEY`. It
   signs nothing without the passphrase: an attempt with an empty one fails with
   `bad decrypt`, and one with no terminal fails outright. It is the only
   private key in the tree. No tracked file of any commit holds private key
   material, and the only files ever tracked under `keys/` are the two public
   copies.

## The finding of item 2

The release script does not refuse a plaintext private key. Step 6 runs
`openssl pkeyutl -sign -inkey "$RELEASE_KEY"`, and openssl asks for a passphrase
only when the PEM is encrypted. A plaintext key signs with no prompt, which a
test with a generated key confirmed. The passphrase is therefore a property of
the file `RELEASE_KEY` names, not of the script. One line before the signing
call would make it a property of the script:

```sh
head -1 "$key" | grep -q 'BEGIN ENCRYPTED PRIVATE KEY' ||
    die "step 6: RELEASE_KEY names no encrypted key"
```

It was not added, because this session changes nothing but the documents outside
the trust-model correction. It is Eddie's call.

## The dry run

`ANTI_SITE=triton.niese.net:/var/www/anti-lang.com/webroot ./r --dry-run` on
`053e508`.

| Step | Outcome |
|---|---|
| 2 | Mac 494, ASan 493, UBSan 493, in an export of the commit |
| 3 | Six packages, levels checked, macos-x86_64 under Rosetta |
| 4 | Six symbols archives, one `SHA256SUMS` of twelve files |
| 5 | anti-linux 431 tests, anti-windows 411 tests, each installs and uninstalls |
| 7 | Would upload 13 files and no signature |
| 9 | Would rsync the two installers, the page, the signature and the key |

The page of the dry run stands in `build/dist/dry-run/downloads-index.html`. It
carries the six release URLs with their digests, the URL of the signature and
the fingerprint of the key.
