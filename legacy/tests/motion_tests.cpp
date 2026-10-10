#include "core/motions.h"
#include "helpers.h"
#include "test.h"

namespace {

Buffer make(const std::string_view text) {
    Buffer b;
    b.set_text(text);
    return b;
}

} // namespace

TEST(motion_word_forward) {
    const Buffer b = make("foo bar.baz\n\n  qux");
    Position p{0, 0};
    p = motion::word_forward(b, p, false);
    CHECK_EQ(p, (Position{0, 4})); // bar
    p = motion::word_forward(b, p, false);
    CHECK_EQ(p, (Position{0, 7})); // .
    p = motion::word_forward(b, p, false);
    CHECK_EQ(p, (Position{0, 8})); // baz
    p = motion::word_forward(b, p, false);
    CHECK_EQ(p, (Position{1, 0})); // empty line counts as a word
    p = motion::word_forward(b, p, false);
    CHECK_EQ(p, (Position{2, 2})); // qux
    p = motion::word_forward(b, p, false);
    CHECK_EQ(p, (Position{2, 5})); // end of buffer
}

TEST(motion_bigword_forward) {
    const Buffer b = make("foo bar.baz qux");
    const Position p = motion::word_forward(b, {0, 4}, true);
    CHECK_EQ(p, (Position{0, 12}));
}

TEST(motion_word_backward) {
    const Buffer b = make("foo bar.baz\n\n  qux");
    Position p{2, 2};
    p = motion::word_backward(b, p, false);
    CHECK_EQ(p, (Position{1, 0}));
    p = motion::word_backward(b, p, false);
    CHECK_EQ(p, (Position{0, 8}));
    p = motion::word_backward(b, p, false);
    CHECK_EQ(p, (Position{0, 7}));
    p = motion::word_backward(b, p, false);
    CHECK_EQ(p, (Position{0, 4}));
    p = motion::word_backward(b, p, false);
    CHECK_EQ(p, (Position{0, 0}));
    p = motion::word_backward(b, p, false);
    CHECK_EQ(p, (Position{0, 0})); // stays at the start
}

TEST(motion_word_end) {
    const Buffer b = make("foo bar\nbaz");
    Position p{0, 0};
    p = motion::word_end(b, p, false);
    CHECK_EQ(p, (Position{0, 2}));
    p = motion::word_end(b, p, false);
    CHECK_EQ(p, (Position{0, 6}));
    p = motion::word_end(b, p, false);
    CHECK_EQ(p, (Position{1, 2}));
}

TEST(motion_word_end_backward) {
    const Buffer b = make("foo bar\nbaz");
    Position p{1, 1};
    p = motion::word_end_backward(b, p, false);
    CHECK_EQ(p, (Position{0, 6}));
    p = motion::word_end_backward(b, p, false);
    CHECK_EQ(p, (Position{0, 2}));
}

TEST(motion_words_utf8) {
    // "é" is two bytes; motions must not land inside it.
    const Buffer b = make("café au lait");
    const Position p = motion::word_forward(b, {0, 0}, false);
    CHECK_EQ(p, (Position{0, 6}));
    CHECK_EQ(motion::word_end(b, {0, 0}, false), (Position{0, 3}));
}

TEST(motion_find_in_line) {
    const std::string line = "a,b,c,d";
    CHECK_EQ(*motion::find_in_line(line, 0, ",", true, false, 1), 1u);
    CHECK_EQ(*motion::find_in_line(line, 0, ",", true, false, 2), 3u);
    CHECK_EQ(*motion::find_in_line(line, 0, ",", true, true, 1), 0u);
    CHECK_EQ(*motion::find_in_line(line, 6, ",", false, false, 1), 5u);
    CHECK_EQ(*motion::find_in_line(line, 6, ",", false, true, 1), 6u);
    CHECK(!motion::find_in_line(line, 0, "x", true, false, 1).has_value());
    // Repeating "t," with ; must not get stuck just before the match.
    CHECK_EQ(*motion::find_in_line(line, 0, ",", true, true, 1, true), 2u);
}
