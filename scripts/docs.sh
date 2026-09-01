#!/usr/bin/env bash
set -euo pipefail

# Build the docs site (strict).
#
# Root-level markdown (README.md, CODE_STYLE.md, ...) is canonical but lives
# outside docs_dir; mkdocs requires all pages under docs_dir. This script
# materializes copies into docs/_root/ (gitignored) and builds. The
# architecture docs in docs/architecture/ are already in place.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/docs/_root"

mkdir -p "$DEST"
for f in \
  README.md \
  CODE_STYLE.md \
  CONTRIBUTING.md \
  DEVELOPMENT.md \
  TESTING.md \
  SECURITY.md \
  ROADMAP.md \
  RISKS.md \
  EXPANSION.md \
  CHANGELOG.md; do
  cp "$ROOT/$f" "$DEST/$f"
done
cp "$ROOT/LICENSE" "$DEST/LICENSE.md"
cp "$ROOT/README.md" "$ROOT/docs/index.md"

exec uv run mkdocs build --strict "$@"
