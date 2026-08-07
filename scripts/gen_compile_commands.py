#!/usr/bin/env python3
"""Regenerate compile_commands.json so clangd resolves each libXXX's
include/ and src/ headers. Run from repo root after adding new source files
or new lib/src directories: python3 scripts/gen_compile_commands.py

clangd's fallback CompileFlags.Add (used when no compile_commands.json
exists) resolves relative -I paths against each source file's own directory,
not the .clangd file's directory — so a bare .clangd config can't find
headers that live in a sibling include/ dir. A real compile_commands.json
sidesteps that by recording each file's actual build directory and flags.
"""
import subprocess, re, json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DIRS = [
    "lib/libcoreipc", "lib/libhtttp", "lib/libmacminidb",
    "lib/libtetrisbrain", "lib/libtetrissh",
    "src/tetrish", "src/tetrish/libft",
    "src/tetrisd", "src/tetrislogd", "src/tetrisu",
]

cc_re = re.compile(r'^\s*(gcc|cc|clang|g\+\+|clang\+\+)\s+.*-c\s')


def main():
    entries = {}
    for rel in DIRS:
        d = os.path.join(ROOT, rel)
        if not os.path.isfile(os.path.join(d, "Makefile")):
            print(f"skip (no Makefile): {rel}", file=sys.stderr)
            continue
        for target in ["all", "test"]:
            try:
                out = subprocess.run(
                    ["make", "-Bnk", target],
                    cwd=d, capture_output=True, text=True, timeout=30,
                ).stdout
            except Exception as e:
                print(f"make -Bnk {target} failed in {rel}: {e}", file=sys.stderr)
                continue
            for line in out.splitlines():
                if not cc_re.match(line):
                    continue
                srcfile = next(
                    (t for t in line.split()
                     if t.endswith(".c") and os.path.isfile(os.path.join(d, t))),
                    None,
                )
                if srcfile is None:
                    continue
                path = os.path.join(d, srcfile)
                entries[path] = {"directory": d, "command": line.strip(), "file": path}

    out_path = os.path.join(ROOT, "compile_commands.json")
    with open(out_path, "w") as f:
        json.dump(list(entries.values()), f, indent=2)
    print(f"wrote {len(entries)} entries to {out_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
