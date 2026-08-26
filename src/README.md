# Source layout

The implementation is split into two independently buildable surfaces:

- `backend/` contains the C++ process-mining library and its native tools.
- `frontend/` contains the React/TypeScript desktop UI and its Vite tooling.

Public C++ headers remain under `include/minera/`; tests and command-line
entry points remain under `tests/` and `tools/` so they can exercise either
surface without depending on internal source paths.
