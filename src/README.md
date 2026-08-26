# Source layout

The implementation is split into two independently buildable surfaces:

- `backend/` contains the C++ process-mining library.
- `backend/tools/` contains native command-line and local-server entry points.
- `backend/tests/` contains the C++ unit and integration tests (and their
  fixtures).
- `frontend/` contains the React/TypeScript desktop UI and its Vite tooling.
- `frontend/tests/` contains the frontend unit tests, separate from production
  modules in `frontend/src/`.

The remaining repository-level directories have deliberately different
ownership: `include/minera/` is the installed public C++ API, `third_party/`
contains vendored dependencies, `scripts/` contains repository automation,
`docs/` contains design and parity notes, and `data/` contains sample inputs.
