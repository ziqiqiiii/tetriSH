# tetriSH umbrella Makefile
#
# Recurses into the self-contained libraries and the vendored shell, then
# provides a shell-driven `run` entry point. Daemons are launched from inside
# the shell via dspawn (see .tetrishrc), not from here.
#
# Targets:
#   make / make all   build libraries, the shell, and any daemons that exist
#   make run          build, then launch the shell (sources .tetrishrc)
#   make stack        build, then launch the daemons headless (integration tests)
#   make clean        recurse `clean` into every component
#   make fclean       recurse `fclean` and drop ./bin
#   make re           fclean + all

SHELL_DIR  := src/tetrish
SHELL_BIN  := $(SHELL_DIR)/macmini_shell
BIN        := bin

# Build only the components that exist yet — the project is in early dev, so
# the daemon directories are filled in over time.
LIB_DIRS    := $(wildcard lib/lib*)
DAEMON_DIRS := $(wildcard src/tetrisd src/tetrislogd src/tetrisctl \
                          src/tetrisu src/chatd src/marketd)

.PHONY: all libs shell daemons bin-link run stack clean fclean reset re

all: libs shell daemons

libs:
	@for d in $(LIB_DIRS); do $(MAKE) -C $$d || exit 1; done

shell:
	@$(MAKE) -C $(SHELL_DIR)

daemons: libs
	@for d in $(DAEMON_DIRS); do $(MAKE) -C $$d || exit 1; done

# Collect every built binary into a single ./bin. The shell prepends $PWD/bin
# to PATH, so dspawn/dcheck and the daemons resolve by name from the prompt.
bin-link: shell daemons
	@mkdir -p $(BIN)
	@ln -sf $(CURDIR)/$(SHELL_DIR)/bin/* $(BIN)/ 2>/dev/null || true

# Idiomatic launch: the shell sources .tetrishrc, which dspawns the daemons.
run: all bin-link
	@TETRISHRC=$(CURDIR)/.tetrishrc ./$(SHELL_BIN)

# Headless stack for integration tests (no interactive shell). Launches only
# the daemons that have been built.
stack: all bin-link
	@for d in tetrislogd tetrisd marketd chatd; do \
		if [ -x $(BIN)/$$d ]; then $(BIN)/$$d & fi; \
	done; \
	echo "Started available daemons; inspect with 'dcheck' or tmp/daemons.reg"

clean:
	@for d in $(LIB_DIRS) $(SHELL_DIR) $(DAEMON_DIRS); do \
		$(MAKE) -C $$d clean >/dev/null 2>&1 || true; \
	done

fclean:
	@for d in $(LIB_DIRS) $(SHELL_DIR) $(DAEMON_DIRS); do \
		$(MAKE) -C $$d fclean >/dev/null 2>&1 || true; \
	done
	@rm -rf $(BIN)

# Like fclean but also wipes daemon runtime state: delegates to the shell's
# own `reset` (drops its tmp/ and archive/) and clears the repo-level bin/tmp.
reset:
	@$(MAKE) -C $(SHELL_DIR) reset >/dev/null 2>&1 || true
	@for d in $(LIB_DIRS) $(DAEMON_DIRS); do \
		$(MAKE) -C $$d fclean >/dev/null 2>&1 || true; \
	done
	@rm -rf $(BIN) tmp

re: fclean all
