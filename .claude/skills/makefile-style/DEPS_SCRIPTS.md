# Dependency logic belongs in a bash script

Dependency **install** and **check** logic does not belong inline in a recipe.
Multi-branch package-manager logic, privilege escalation, source builds, and
compile/link probes are real shell programs — control flow, temp files, traps —
and a Makefile recipe is one shell line per step with awkward `\`-continuations
and no real control flow. Move all of it into a bash script under the
component's `scripts/` directory and have the recipe just call it:

```make
install-deps:
	@ bash ./scripts/install_deps.sh

install-notcurses-from-source:
	@ bash ./scripts/install_notcurses.sh

check-deps:
	@ UNAME_S=$(UNAME_S) REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		bash ./scripts/check_deps.sh

deps-info:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) bash ./scripts/deps_info.sh

# Orchestration also moves out; it delegates to check_deps.sh / install_deps.sh.
deps:
	@ UNAME_S=$(UNAME_S) AUTO_INSTALL_DEPS=$(AUTO_INSTALL_DEPS) \
		REQUIRE_VALGRIND=$(REQUIRE_VALGRIND) \
		GREEN='$(GREEN)' CLR_RMV='$(CLR_RMV)' bash ./scripts/deps.sh
```

- The recipe is a **single `bash ./scripts/*.sh` line** — no `case`, no `if`, no
  package lists or probe here-docs in the Makefile. Pass anything variable
  through the environment rather than templating it into the script:
  `@ NOTCURSES_VERSION=$(NOTCURSES_VERSION) bash ./scripts/install_notcurses.sh`.
- Scripts live in **`scripts/` next to the Makefile that calls them**
  (`src/tetrisu/scripts/install_deps.sh`), keeping each component self-contained.
- **Install, check, and the orchestration between them** all move out once they
  grow past a line or two — install is where the branching and privilege live; a
  check that does a real compile/link probe (temp file + `trap` cleanup) is a
  program too; and the `deps` flow (probe → install on failure → print the status
  line) is control flow that reads better in a script than in
  backslash-continued `if`/`elif`. The orchestration script **delegates to the
  sibling scripts directly** rather than re-entering `make`. A one-liner with no
  temp files, loops, or branching can stay inline; anything longer moves out.
- **Colour escapes stay owned by the Makefile.** A script that prints a coloured
  line receives the escapes through the environment
  (`GREEN='$(GREEN)' CLR_RMV='$(CLR_RMV)'`) and emits them with `printf '%b'`, so
  the Makefile stays the single source of the palette.

---

## Privilege detection

Handled *inside the script*, once, at the top. Detect it: nothing if already
root, `sudo` if available, a clear failure otherwise.

```bash
#!/usr/bin/env bash
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
elif command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
else
    echo "Root access or sudo is required to install packages." >&2
    exit 1
fi

# ... then prefix every privileged command:
$SUDO apt-get update
$SUDO apt-get install -y build-essential pkg-config libssl-dev
```

---

## Script rules

- `#!/usr/bin/env bash` and `set -euo pipefail` on every install script.
- Resolve `$SUDO` **once** at the top; prefix every privileged command with
  `$SUDO` (unquoted, so an empty value expands to nothing when already root).
- `sudo` itself performs the interactive password prompt — the script neither
  reads nor stores credentials.
- Detect the package manager (`command -v apt-get`, `dnf`, `pacman`, …) and
  branch inside the script; on an unsupported platform print an actionable
  message and `exit 1`, never a silent no-op.
- Clean up temp directories with `trap '...' EXIT HUP INT TERM` (source builds).
