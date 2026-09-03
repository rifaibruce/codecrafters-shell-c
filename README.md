# shell

A POSIX-style command-line shell written from scratch in C, built through the
CodeCrafters "Build Your Own Shell" challenge.

No `system()`, no wrapping an existing shell — input is tokenized by hand,
`PATH` is resolved manually, and external programs run via `fork()`/`execv()`.

## Features

**Built-ins:** `echo`, `exit`, `type`, `cd`, `pwd`

**External commands** — resolved by scanning `PATH` and checking execute
permission with `access()`, then executed in a forked child.

**Quoting and escapes**

- Single quotes preserve everything literally
- Double quotes with backslash escapes for `"`, `\`, `$`, `` ` ``, and newline
- Backslash escaping outside of quotes

**Output redirection**

- `>` / `1>` — stdout, truncate
- `2>` — stderr, truncate
- `>>` / `1>>` — stdout, append
- `2>>` — stderr, append

**Interactive editing** via GNU Readline — line editing, command history, and
tab completion.

**Tab completion** — completes built-ins, then walks every directory on `PATH`
to complete external command names.

**`cd` conveniences** — bare `cd` or `cd ~` goes to `$HOME`; `~/foo` expands.

## Building

Requires `cmake` and GNU Readline.

```sh
cmake -B build
cmake --build build
./build/shell
```

## Usage

```
$ echo "hello   world"
hello   world
$ type echo
echo is a shell builtin
$ ls -la > files.txt
$ ls nonexistent 2>> errors.log
$ cd ~/projects
```

## Implementation notes

**Parsing** is a single pass over the input with a small state machine —
`quote_mode_flag` tracks whether the scanner is outside quotes, inside single
quotes, or inside double quotes, and each state has its own rules for what a
backslash means. Getting this right was the fiddliest part of the project: the
escape rules differ between the three contexts, and a quoted string containing
spaces has to survive word splitting as a single token.

**Tab completion** was the most interesting piece to build. Readline's
completion API expects a generator function that is called repeatedly and
returns one match per call, returning `NULL` when exhausted — so the traversal
state has to persist across calls in `static` variables rather than living on
the stack. The generator walks the built-in list first, then opens each `PATH`
directory in turn with `opendir()`/`readdir()`, resuming exactly where the
previous call left off.

**Redirection** is set up before dispatch by `dup2()`-ing the target file
descriptor over stdout or stderr, saving the original with `dup()` so it can be
restored after the command finishes. Because this happens before `fork()`, the
child inherits the redirection without any extra work.

## Known issues

- Token and argument buffers are fixed-size without bounds checks.
- Allocations from `strdup()` and the argv array are not freed between commands.

## Not implemented

Pipes, input redirection (`<`), background jobs (`&`), signal handling, and
environment variable expansion.
