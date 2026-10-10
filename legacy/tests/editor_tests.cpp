#include "helpers.h"
#include "test.h"

#include <filesystem>

// ---- Movement
// ------------------------------------------------------------------

TEST(editor_hjkl) {
    Session s("abc\ndef\nghi");
    s.keys("jl");
    CHECK_EQ(s.cursor(), (Position{1, 1}));
    s.keys("kh");
    CHECK_EQ(s.cursor(), (Position{0, 0}));
    s.keys("h"); // cannot go further left
    CHECK_EQ(s.cursor(), (Position{0, 0}));
    s.keys("llll"); // stops on the last character
    CHECK_EQ(s.cursor(), (Position{0, 2}));
}

TEST(editor_counts) {
    Session s("0123456789\na\nb\nc\nd");
    s.keys("5l");
    CHECK_EQ(s.cursor(), (Position{0, 5}));
    s.keys("3j");
    CHECK_EQ(s.cursor(), (Position{3, 0}));
}

TEST(editor_vertical_motion_keeps_column) {
    Session s("long line here\nab\nanother long line");
    s.keys("10l");
    s.keys("j");
    CHECK_EQ(s.cursor(), (Position{1, 1})); // clamped on the short line
    s.keys("j");
    CHECK_EQ(s.cursor(), (Position{2, 10})); // column restored
}

TEST(editor_dollar_sticks_to_end_of_line) {
    Session s("abc\nlonger line\nxy");
    s.keys("$j");
    CHECK_EQ(s.cursor(), (Position{1, 10}));
    s.keys("j");
    CHECK_EQ(s.cursor(), (Position{2, 1}));
}

TEST(editor_line_motions) {
    Session s("   indented text");
    s.keys("$");
    CHECK_EQ(s.cursor(), (Position{0, 15}));
    s.keys("0");
    CHECK_EQ(s.cursor(), (Position{0, 0}));
    s.keys("^");
    CHECK_EQ(s.cursor(), (Position{0, 3}));
}

TEST(editor_gg_and_G) {
    Session s("a\n  b\nc\nd");
    s.keys("G");
    CHECK_EQ(s.cursor(), (Position{3, 0}));
    s.keys("gg");
    CHECK_EQ(s.cursor(), (Position{0, 0}));
    s.keys("2G");
    CHECK_EQ(s.cursor(), (Position{1, 2}));
    s.keys("3gg");
    CHECK_EQ(s.cursor(), (Position{2, 0}));
}

TEST(editor_find_char) {
    Session s("a,b,c,d");
    s.keys("f,");
    CHECK_EQ(s.cursor(), (Position{0, 1}));
    s.keys(";");
    CHECK_EQ(s.cursor(), (Position{0, 3}));
    s.keys(",");
    CHECK_EQ(s.cursor(), (Position{0, 1}));
    s.keys("$F,");
    CHECK_EQ(s.cursor(), (Position{0, 5}));
    s.keys("0t,;");
    CHECK_EQ(s.cursor(), (Position{0, 2}));
}

TEST(editor_search) {
    Session s("one\ntwo foo\nthree foo\nfour");
    s.keys("/foo<CR>");
    CHECK_EQ(s.cursor(), (Position{1, 4}));
    s.keys("n");
    CHECK_EQ(s.cursor(), (Position{2, 6}));
    s.keys("n"); // wraps around
    CHECK_EQ(s.cursor(), (Position{1, 4}));
    CHECK(!s.editor.message().empty());
    s.keys("N");
    CHECK_EQ(s.cursor(), (Position{2, 6}));
    s.keys("gg?two<CR>");
    CHECK_EQ(s.cursor(), (Position{1, 0}));
}

TEST(editor_search_not_found) {
    Session s("abc");
    s.keys("/zzz<CR>");
    CHECK(s.editor.message_is_error());
    CHECK_EQ(s.cursor(), (Position{0, 0}));
}

// ---- Insert mode
// ---------------------------------------------------------------

TEST(editor_insert_and_escape) {
    Session s("world");
    s.keys("ihello <Esc>");
    CHECK_EQ(s.text(), "hello world");
    CHECK(s.editor.mode() == Mode::Normal);
    CHECK_EQ(s.cursor(), (Position{0, 5})); // Esc moves left one
}

TEST(editor_append_variants) {
    CHECK_EQ(run("abc", "aX<Esc>"), "aXbc");
    CHECK_EQ(run("abc", "AX<Esc>"), "abcX");
    CHECK_EQ(run("  abc", "$IX<Esc>"), "  Xabc");
    CHECK_EQ(run("", "aX<Esc>"), "X");
}

TEST(editor_open_line) {
    CHECK_EQ(run("a\nc", "ob<Esc>"), "a\nb\nc");
    CHECK_EQ(run("b\nc", "Oa<Esc>"), "a\nb\nc");
    // Autoindent copies the current line's indentation.
    CHECK_EQ(run("    a", "ob<Esc>"), "    a\n    b");
}

TEST(editor_insert_enter_and_backspace) {
    CHECK_EQ(run("abcd", "lli<CR><Esc>"), "ab\ncd");
    CHECK_EQ(run("ab\ncd", "ji<BS><Esc>"), "abcd");
    CHECK_EQ(run("abc", "A<BS><BS><Esc>"), "a");
    CHECK_EQ(run("ab\ncd", "A<Del><Esc>"), "abcd");
}

TEST(editor_insert_tab_expands) {
    CHECK_EQ(run("x", "i<Tab><Esc>"), "    x");
    Session s("x");
    s.editor.options().expandtab = false;
    s.keys("i<Tab><Esc>");
    CHECK_EQ(s.text(), "\tx");
}

TEST(editor_insert_utf8) {
    Session s("ab");
    s.keys("aé<Esc>");
    CHECK_EQ(s.text(), "aéb");
    CHECK_EQ(s.cursor(), (Position{0, 1}));
    s.keys("lx");
    CHECK_EQ(s.text(), "aé");
}

// ---- Operators
// -----------------------------------------------------------------

TEST(editor_delete_word) {
    CHECK_EQ(run("foo bar baz", "dw"), "bar baz");
    CHECK_EQ(run("foo bar baz", "2dw"), "baz");
    CHECK_EQ(run("foo bar baz", "d2w"), "baz");
    // The last word on a line: stop at the end of the line.
    CHECK_EQ(run("foo bar\nnext", "wdw"), "foo \nnext");
}

TEST(editor_delete_motions) {
    CHECK_EQ(run("foo bar baz", "wde"), "foo  baz");
    CHECK_EQ(run("foo bar baz", "$db"), "foo bar z");
    CHECK_EQ(run("foo bar baz", "wd$"), "foo ");
    CHECK_EQ(run("foo bar baz", "wD"), "foo ");
    CHECK_EQ(run("foo bar baz", "$d0"), "z");
    CHECK_EQ(run("a,b,c", "dt,"), ",b,c");
    CHECK_EQ(run("a,b,c", "df,"), "b,c");
}

TEST(editor_delete_lines) {
    CHECK_EQ(run("a\nb\nc", "dd"), "b\nc");
    CHECK_EQ(run("a\nb\nc", "jdd"), "a\nc");
    CHECK_EQ(run("a\nb\nc", "Gdd"), "a\nb");
    CHECK_EQ(run("a\nb\nc", "2dd"), "c");
    CHECK_EQ(run("a\nb\nc", "dj"), "c");
    CHECK_EQ(run("a\nb\nc", "Gdk"), "a");
    CHECK_EQ(run("a\nb\nc", "dG"), "");
    CHECK_EQ(run("a\nb\nc", "jdgg"), "c");
    CHECK_EQ(run("only", "dd"), "");
}

TEST(editor_dd_cursor_position) {
    Session s("a\n  b\nc");
    s.keys("dd");
    CHECK_EQ(s.cursor(), (Position{0, 2})); // first non-blank of new line
    s.keys("Gdd");
    CHECK_EQ(s.cursor(), (Position{0, 2}));
}

TEST(editor_x_and_X) {
    CHECK_EQ(run("abc", "x"), "bc");
    CHECK_EQ(run("abc", "3x"), "");
    CHECK_EQ(run("abc", "$x"), "ab");
    CHECK_EQ(run("abc", "$X"), "ac");
    CHECK_EQ(run("", "x"), "");
    Session s("abc");
    s.keys("$x");
    CHECK_EQ(s.cursor(), (Position{0, 1})); // stays on a character
}

TEST(editor_change) {
    CHECK_EQ(run("foo bar", "cwbaz<Esc>"), "baz bar");
    CHECK_EQ(run("foo bar", "c2wx<Esc>"), "x");
    CHECK_EQ(run("a b", "cwX<Esc>"), "X b"); // single-letter word
    CHECK_EQ(run("foo bar", "wCx<Esc>"), "foo x");
    CHECK_EQ(run("foo bar", "sX<Esc>"), "Xoo bar");
    CHECK_EQ(run("    foo\nbar", "ccX<Esc>"), "    X\nbar");
    CHECK_EQ(run("foo\nbar", "SX<Esc>"), "X\nbar");
}

TEST(editor_change_is_one_undo_step) {
    Session s("foo bar");
    s.keys("cwhello there<Esc>");
    CHECK_EQ(s.text(), "hello there bar");
    s.keys("u");
    CHECK_EQ(s.text(), "foo bar");
}

TEST(editor_yank_and_paste) {
    CHECK_EQ(run("foo bar", "ywP"), "foo foo bar");
    CHECK_EQ(run("foo bar", "yw$p"), "foo barfoo ");
    CHECK_EQ(run("a\nb", "yyp"), "a\na\nb");
    CHECK_EQ(run("a\nb", "yyjP"), "a\na\nb");
    CHECK_EQ(run("a\nb", "Yjp"), "a\nb\na");
    CHECK_EQ(run("a\nb", "yj2p"), "a\na\nb\na\nb\nb");
    CHECK_EQ(run("ab", "xp"), "ba");
    CHECK_EQ(run("a\nb", "ddp"), "b\na");
}

TEST(editor_yank_does_not_modify) {
    Session s("abc");
    s.keys("yy");
    CHECK(!s.editor.buffer().is_modified());
}

TEST(editor_named_registers) {
    Session s("one\ntwo");
    s.keys("\"ayyj\"byy");
    s.keys("\"ap\"bp");
    CHECK_EQ(s.text(), "one\ntwo\none\ntwo");
    // Deleting into the black hole register keeps the unnamed register.
    Session t("keep\ntrash");
    t.keys("yyj\"_ddP");
    CHECK_EQ(t.text(), "keep\nkeep");
}

TEST(editor_paste_empty_register_errors) {
    Session s("abc");
    s.keys("\"zp");
    CHECK(s.editor.message_is_error());
    CHECK_EQ(s.text(), "abc");
}

// ---- Other commands
// ------------------------------------------------------------

TEST(editor_join) {
    CHECK_EQ(run("a\n   b", "J"), "a b");
    CHECK_EQ(run("a\nb\nc", "3J"), "a b c");
    CHECK_EQ(run("a \nb", "J"), "a b");
    CHECK_EQ(run("a\n", "J"), "a");
    CHECK_EQ(run("f(\n)", "J"), "f()");
}

TEST(editor_replace_char) {
    CHECK_EQ(run("abc", "rx"), "xbc");
    CHECK_EQ(run("abc", "2rx"), "xxc");
    CHECK_EQ(run("abc", "5rx"), "abc"); // not enough characters
    CHECK_EQ(run("abc", "r<Esc>"), "abc");
}

TEST(editor_undo_redo) {
    Session s("abc");
    s.keys("x");
    s.keys("x");
    CHECK_EQ(s.text(), "c");
    s.keys("u");
    CHECK_EQ(s.text(), "bc");
    s.keys("u");
    CHECK_EQ(s.text(), "abc");
    CHECK(!s.editor.buffer().is_modified());
    s.keys("<C-r>");
    CHECK_EQ(s.text(), "bc");
    s.keys("2u");
    CHECK_EQ(s.text(), "abc");
}

TEST(editor_undo_restores_cursor) {
    Session s("a\nb\nc");
    s.keys("jdd");
    CHECK_EQ(s.cursor(), (Position{1, 0}));
    s.keys("ggu");
    CHECK_EQ(s.text(), "a\nb\nc");
    CHECK_EQ(s.cursor(), (Position{1, 0}));
}

TEST(editor_escape_cancels_pending) {
    Session s("abc");
    s.keys("d<Esc>x");
    CHECK_EQ(s.text(), "bc");
    CHECK_EQ(s.editor.pending_keys(), "");
    s.keys("2");
    CHECK_EQ(s.editor.pending_keys(), "2");
}

TEST(editor_invalid_command_is_ignored) {
    Session s("abc");
    s.keys("dQx");
    CHECK_EQ(s.text(), "bc");
}

// ---- Visual mode
// ---------------------------------------------------------------

TEST(editor_visual_delete) {
    CHECK_EQ(run("hello world", "vlld"), "lo world");
    CHECK_EQ(run("hello world", "wvbd"), "orld"); // backwards selection
    CHECK_EQ(run("ab\ncd\nef", "lvjd"), "a\nef");
}

TEST(editor_visual_line) {
    CHECK_EQ(run("a\nb\nc\nd", "jVjd"), "a\nd");
    CHECK_EQ(run("a\nb\nc", "Vjyjjp"), "a\nb\nc\na\nb");
}

TEST(editor_visual_change_and_swap) {
    CHECK_EQ(run("foo bar", "veciX<Esc>"), "iX bar");
    Session s("abcdef");
    s.keys("lvll");
    s.keys("o");
    CHECK_EQ(s.cursor(), (Position{0, 1}));
    s.keys("hd");
    CHECK_EQ(s.text(), "ef");
}

TEST(editor_visual_escape) {
    Session s("abc");
    s.keys("vl");
    CHECK(s.editor.selection().has_value());
    s.keys("<Esc>");
    CHECK(s.editor.mode() == Mode::Normal);
    CHECK(!s.editor.selection().has_value());
    CHECK_EQ(s.text(), "abc");
}

// ---- Ex commands
// ---------------------------------------------------------------

TEST(editor_goto_line_command) {
    Session s("a\nb\nc\nd");
    s.keys(":3<CR>");
    CHECK_EQ(s.cursor(), (Position{2, 0}));
    s.keys(":$<CR>");
    CHECK_EQ(s.cursor(), (Position{3, 0}));
}

TEST(editor_quit_refuses_unsaved_changes) {
    Session s("abc");
    s.keys("x:q<CR>");
    CHECK(!s.editor.should_quit());
    CHECK(s.editor.message_is_error());
    s.keys(":q!<CR>");
    CHECK(s.editor.should_quit());
}

TEST(editor_quit_when_clean) {
    Session s("abc");
    s.keys(":q<CR>");
    CHECK(s.editor.should_quit());
}

TEST(editor_write_and_quit) {
    const auto path =
        std::filesystem::temp_directory_path() / "cursey_editor_test.txt";
    std::filesystem::remove(path);
    Editor editor;
    editor.open(path.string());
    for (const int k : key::parse("ihello<Esc>:wq<CR>")) {
        editor.handle_key(k);
    }
    CHECK(editor.should_quit());
    Buffer reread;
    std::string error;
    CHECK(reread.load(path.string(), error));
    CHECK_EQ(reread.text(), "hello");
    std::filesystem::remove(path);
}

TEST(editor_set_option) {
    Session s("x");
    s.keys(":set ts=8 noet<CR>");
    CHECK_EQ(s.editor.options().tabstop, 8u);
    CHECK(!s.editor.options().expandtab);
    s.keys(":set bogus<CR>");
    CHECK(s.editor.message_is_error());
    s.keys(":se ts?<CR>");
    CHECK_EQ(s.editor.message(), "tabstop=8");
}

TEST(editor_unknown_command) {
    Session s("x");
    s.keys(":frobnicate<CR>");
    CHECK(s.editor.message_is_error());
}

TEST(editor_command_line_backspace_cancels) {
    Session s("x");
    s.keys(":<BS>");
    CHECK(s.editor.mode() == Mode::Normal);
    s.keys(":ab<BS>");
    CHECK_EQ(s.editor.command_line(), ":a");
}

// ---- Scrolling
// -----------------------------------------------------------------

TEST(editor_scrolls_to_keep_cursor_visible) {
    std::string text;
    for (int i = 0; i < 100; ++i) {
        text += "line " + std::to_string(i) + "\n";
    }
    Session s(text);
    s.editor.set_view_size(10, 80);
    s.keys("50G");
    const std::size_t top = s.editor.view_top();
    CHECK(49 >= top && 49 < top + 10);
    s.keys("gg");
    CHECK_EQ(s.editor.view_top(), 0u);
}

TEST(editor_half_page_scroll) {
    std::string text;
    for (int i = 0; i < 100; ++i) {
        text += "line\n";
    }
    Session s(text);
    s.editor.set_view_size(20, 80);
    s.keys("<C-d>");
    CHECK_EQ(s.cursor().row, 10u);
    s.keys("<C-u>");
    CHECK_EQ(s.cursor().row, 0u);
}
