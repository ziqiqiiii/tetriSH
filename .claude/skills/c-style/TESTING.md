# Tests

Each self-contained component owns its tests under `tests/`, plus a shared
`scripts/run_tests.sh` that runs them and formats the output. Two kinds:

- **Unit tests** — compiled C, one file per source module, named
  `tests/test_<module>.c` (e.g. `test_board.c`, `test_abilities.c`). Each builds
  into `tests/bin/` and links the library archive directly.
- **Integration tests** — shell scripts driven through the same runner in
  `integration` mode.

---

## Test file layout

Mirrors the source conventions: a leading comment banner describing the suite,
then one `void test_<description>(void)` per case, with `static` helpers (e.g.
`fill_row`, `fill_board`) shared within the file. Cases use `assert()` and report
on stdout in the runner's protocol:

```c
void	test_cut_top_zero_is_noop(void)
{
	t_board	before;
	t_board	after;

	...
	assert(memcmp(&before, &after, sizeof(t_board)) == 0);
	printf("PASS test_cut_top_zero_is_noop\n");
}
```

---

## Output protocol

Parsed by `run_tests.sh`:

- Unit tests print `PASS <name>` / `FAIL <name>` per case (Unity-style
  `file:line:name:PASS|FAIL[:detail]` is also recognised).
- Integration scripts print `PASS: <desc>` / `FAIL: <desc>`.
- A test binary or script signals overall failure with a non-zero exit code; the
  runner exits 0 only if every test passed.

---

## Running

```bash
make -C lib/libXXX test                   # build every test_*.c and run them
make -C lib/libXXX test FILTER=abilities  # only tests whose path contains the pattern
```

`FILTER=<substring>` is matched against each test's path, so it selects a module
(`FILTER=board`) or a stage (`FILTER=04`). No match → the runner reports it and
exits non-zero.

Test binaries are expected to pass
`valgrind --leak-check=full --error-exitcode=1`.
