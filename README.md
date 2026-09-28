# cursey

A small vim-like text editor in C++20, using [notcurses](https://github.com/dankamongmen/notcurses) for the terminal.

## Build

```sh
brew install cmake pkg-config notcurses      # macOS
cmake -S . -B build
cmake --build build
./build/cursey file.txt                      # run the editor
./build/cursey_tests                         # run the unit tests
```

Other options:

- `-DCURSEY_SANITIZE=ON` builds with AddressSanitizer and UBSan.
- `CURSEY_LOG=/tmp/cursey.log ./build/cursey file.txt` writes debug logs to that file (see `src/utils/log.h`).

## What works

| Mode | Keys |
|------|------|
| Normal: motions | `h j k l` / arrows, `w b e ge W B E gE`, `0 ^ $`, `gg G` (with counts, e.g. `12G`), `f F t T ; ,`, `+ - <CR>`, `/pattern`, `?pattern`, `n N` |
| Normal: operators | `d c y` + any motion, `dd cc yy`, counts in both places (`2d3w`) |
| Normal: commands | `x X D C s S Y p P J r u <C-r> <C-d> <C-u>`, `"a`–`"z` registers (`"A` appends, `"_` discards) |
| Insert | `i a I A o O`, text, `<CR>` (auto-indent), `<BS>`, `<Del>`, `<Tab>`, arrows, `<Esc>` / `<C-c>` |
| Visual | `v`, `V`, any motion, `d y c o` |
| Ex | `:w [file]`, `:q`, `:q!`, `:wq`, `:x`, `:e[!] [file]`, `:N`, `:$`, `:set ts=4 sw=4 et ai nu so=5` |

Every command is one undo step. Text typed in insert mode joins the command that started it, so `cwfoo<Esc>` undoes as a single step.

## Layout

```
main.cpp                 main loop: render -> read key -> editor.handle_key()
src/core/                editor logic (no terminal code; fully unit-testable)
  types.h                Position, Mode
  buffer.{h,cpp}         lines of text + undo/redo; all edits go through
                         insert() and erase(), which record themselves
  editor.{h,cpp}         modes, the normal-mode command parser, operators
  motions.{h,cpp}        word motions and f/t as pure functions
  registers.{h,cpp}      "", "0, "1-"9, "-, "a-"z, "_
  options.{h,cpp}        :set options
  keys.{h,cpp}           key codes + vim notation parser ("d2w", "<Esc>")
src/commands/            ":" commands (a table: name -> handler)
src/ui/                  notcurses front end + C/C++ syntax highlighting
src/utils/               UTF-8 helpers, debug logger
tests/                   unit tests (tiny built-in framework, tests/test.h)
```

The main design rule: **`Editor` never touches the terminal.** The UI sends it keys and reads back state (`cursor()`, `mode()`, `selection()`, `message()`, ...). So a feature can be tested by sending keys and checking the text:

```cpp
TEST(editor_delete_word) {
    CHECK_EQ(run("foo bar baz", "dw"), "bar baz");
}
```

### How normal mode parses keys

Keys collect in `m_pending`. After each key, `run_normal_command()` parses the whole sequence again from the start, following vim's grammar `["x][count]operator[count]motion`. It returns one of three results:

- **Incomplete:** wait for more keys, e.g. after `d` or `2d`.
- **Invalid:** throw the keys away.
- **Done:** the command ran.

Because it re-parses from scratch, the parser keeps no state between key presses.

### Extending it

- **New motion:** add its key to `parse_motion()`. Then add a `case` in `eval_motion()` that returns a `MotionTarget` (position plus exclusive, inclusive or linewise). It then works on its own, with counts, with `d`/`c`/`y`, and in visual mode.
- **New operator (e.g. `>`, `gu`):** accept it next to `d c y` in `run_normal_command()` and add a branch in `apply_operator()`.
- **New `:` command:** write a handler in `src/commands/ex_commands.cpp` and add a row to the `commands` table.
- **New option:** add a field to `Options` and a row to `option_defs` in `options.cpp`.

## Ideas for next steps

Roughly from easiest to hardest:

- `>>` / `<<` and `>`/`<` operators (indent), `~`, `gu`/`gU`
- `.` repeat: record the keys of the last change and replay them through `handle_key`
- Text objects (`iw`, `aw`, `i"`, `i(`, ...): return a range from `eval_motion` when `i`/`a` follows an operator
- `%`, `{` / `}`, `*` / `#`
- Counts for insert (`3ihi<Esc>`) and `R` replace mode
- Command-line history and cursor movement; `:s/pat/rep/g` with ranges
- Macros (`q`, `@`): keys are plain codepoints, so a register can hold them as UTF-8 (`key::to_text`)
- Highlighting search matches; multi-line comment state in the highlighter
- Multiple buffers / splits
- Faster buffer for huge files (a rope or piece table behind the same `Buffer` interface)
