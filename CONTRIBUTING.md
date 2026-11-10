# Contributing

Thanks for your interest in Minera. Issues and pull requests are welcome.

## Before you start

- For a bug, open an issue with the input that triggers it (a small CSV, XES
  or OCEL file is ideal), what you expected and what happened.
- For a larger change, open an issue first so we can agree on the approach.
- Security problems go through private reporting; see [SECURITY.md](SECURITY.md).

## Making a change

1. Build and run the tests as described in the [README](README.md#build-and-test).
2. Keep behaviour that is bound to the reference release unchanged unless the
   change fixes a parity bug. The [parity contract](docs/reference-2.7.22-parity.md)
   says which results must match exactly and which are checked by invariants.
3. Add or update tests next to the code you touch: `src/backend/tests/` for
   the C++ core, `src/frontend/tests/` for the app, `scripts/tests/` for the
   Python tooling.
4. Before opening the pull request, run `ctest --test-dir build`, plus
   `npx tsc --noEmit` and `npx vitest run` in `src/frontend` when you change
   the app.

## Licence

Minera is licensed under the [GNU AGPL v3.0 or later](LICENSE). By submitting
a contribution you agree that it is licensed under the same terms.
