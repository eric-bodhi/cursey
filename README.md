# cursey

A small vim-like text editor in Rust, using [ratatui](https://ratatui.rs) and [crossterm](https://github.com/crossterm-rs/crossterm) for the terminal.

The original C++20/notcurses version lives in [`legacy/`](legacy/) (build it with `cmake -S legacy -B legacy/build`).

## Build

```sh
cargo run -- file.txt                        # run the editor
cargo test                                   # run the unit tests
cargo build --release                        # optimized binary in target/release/cursey
```

`CURSEY_LOG=/tmp/cursey.log cargo run -- file.txt` writes debug logs to that file (see `core/src/log.rs`).

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

Lines are stored as UTF-8 `String`s. A file that is not valid UTF-8 opens with the bad bytes replaced by `�` and a warning, so saving it changes those bytes.

## Layout

The project is a Cargo workspace with two crates:

```
src/                     the `cursey` binary: terminal front end
  main.rs                main loop: render -> read key -> editor.handle_key()
  tui.rs                 ratatui drawing + crossterm key input
  lex.rs                 C/C++ syntax highlighting
core/                    the `cursey-core` library: editor logic, no terminal
  src/types.rs           Position, Mode
  src/buffer.rs          lines of text + undo/redo; all edits go through
                         insert() and erase(), which record themselves
  src/editor.rs          modes, the normal-mode command parser, operators
  src/motions.rs         word motions and f/t as pure functions
  src/registers.rs       "", "0, "1-"9, "-, "a-"z, "_
  src/options.rs         :set options
  src/keys.rs            key codes + vim notation parser ("d2w", "<Esc>")
  src/ex.rs              ":" commands (a table: name -> handler)
  src/utf8.rs            UTF-8 / display-width helpers
  src/log.rs             debug logger
  tests/                 unit tests driving the editor with key notation
legacy/                  the previous C++ implementation
```

The main design rule: **`Editor` never touches the terminal.** The compiler enforces it: `cursey-core` does not depend on ratatui at all. The UI sends the editor keys and reads back state (`cursor()`, `mode()`, `selection()`, `message()`, ...). So a feature can be tested by sending keys and checking the text:

```rust
#[test]
fn editor_delete_word() {
    assert_eq!(run("foo bar baz", "dw"), "bar baz");
}
```

### How normal mode parses keys

Keys collect in `pending`. After each key, `run_normal_command()` parses the whole sequence again from the start, following vim's grammar `["x][count]operator[count]motion`. It returns one of three results:

- **`Err(Incomplete)`:** wait for more keys, e.g. after `d` or `2d`.
- **`Err(Invalid)`:** throw the keys away.
- **`Ok(())`:** the command ran.

Because it re-parses from scratch, the parser keeps no state between key presses.

### Extending it

- **New motion:** add its key to `parse_motion()`. Then add an arm in `eval_motion()` that returns a `MotionTarget` (position plus exclusive, inclusive or linewise). It then works on its own, with counts, with `d`/`c`/`y`, and in visual mode.
- **New operator (e.g. `>`, `gu`):** accept it next to `d c y` in `run_normal_command()` and add a branch in `apply_operator()`.
- **New `:` command:** write a handler in `core/src/ex.rs` and add a row to the `COMMANDS` table.
- **New option:** add a field to `Options` and a row to `OPTION_DEFS` in `core/src/options.rs`.

## Ideas for next steps

Roughly from easiest to hardest:

- `>>` / `<<` and `>`/`<` operators (indent), `~`, `gu`/`gU`
- `.` repeat: record the keys of the last change and replay them through `handle_key`
- Text objects (`iw`, `aw`, `i"`, `i(`, ...): return a range from `eval_motion` when `i`/`a` follows an operator
- `%`, `{` / `}`, `*` / `#`
- Counts for insert (`3ihi<Esc>`) and `R` replace mode
- Command-line history and cursor movement; `:s/pat/rep/g` with ranges
- Macros (`q`, `@`): keys are plain `char`s, so a register can hold them as UTF-8 (`keys::to_text`)
- Highlighting search matches; multi-line comment state in the highlighter; Rust highlighting
- Multiple buffers / splits
- Faster buffer for huge files (a rope or piece table behind the same `Buffer` interface)
