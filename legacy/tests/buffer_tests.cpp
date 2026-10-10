#include "core/buffer.h"
#include "helpers.h"
#include "test.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

Buffer make(const std::string_view text) {
    Buffer b;
    b.set_text(text);
    return b;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

TEST(buffer_starts_with_one_empty_line) {
    const Buffer b;
    CHECK_EQ(b.line_count(), 1u);
    CHECK_EQ(b.line(0), "");
    CHECK(!b.is_modified());
}

TEST(buffer_insert_single_line) {
    Buffer b = make("hello");
    const Position end = b.insert({0, 5}, " world");
    CHECK_EQ(b.text(), "hello world");
    CHECK_EQ(end, (Position{0, 11}));
}

TEST(buffer_insert_multi_line) {
    Buffer b = make("abcd");
    const Position end = b.insert({0, 2}, "X\nY\nZ");
    CHECK_EQ(b.text(), "abX\nY\nZcd");
    CHECK_EQ(end, (Position{2, 1}));
}

TEST(buffer_erase_across_lines) {
    Buffer b = make("one\ntwo\nthree");
    const std::string removed = b.erase({0, 1}, {2, 2});
    CHECK_EQ(removed, "ne\ntwo\nth");
    CHECK_EQ(b.text(), "oree");
}

TEST(buffer_erase_line_break) {
    Buffer b = make("ab\ncd");
    b.erase({0, 2}, {1, 0});
    CHECK_EQ(b.text(), "abcd");
}

TEST(buffer_delete_lines) {
    Buffer middle = make("a\nb\nc");
    CHECK_EQ(middle.delete_lines(1, 1), "b");
    CHECK_EQ(middle.text(), "a\nc");

    Buffer last = make("a\nb\nc");
    CHECK_EQ(last.delete_lines(1, 2), "b\nc");
    CHECK_EQ(last.text(), "a");

    Buffer all = make("a\nb");
    all.delete_lines(0, 1);
    CHECK_EQ(all.line_count(), 1u);
    CHECK_EQ(all.text(), "");
}

TEST(buffer_insert_lines) {
    Buffer b = make("a\nc");
    b.insert_lines(1, "b");
    b.insert_lines(3, "d");
    b.insert_lines(0, "start");
    CHECK_EQ(b.text(), "start\na\nb\nc\nd");
}

TEST(buffer_undo_redo_group) {
    Buffer b = make("hello");
    b.begin_undo_group({0, 0});
    b.insert({0, 5}, " world");
    b.erase({0, 0}, {0, 1});
    b.end_undo_group();
    CHECK_EQ(b.text(), "ello world");

    CHECK(b.undo().has_value());
    CHECK_EQ(b.text(), "hello");
    CHECK(!b.undo().has_value());

    CHECK(b.redo().has_value());
    CHECK_EQ(b.text(), "ello world");
    CHECK(!b.redo().has_value());
}

TEST(buffer_new_edit_clears_redo) {
    Buffer b = make("a");
    b.insert({0, 1}, "b");
    b.undo();
    b.insert({0, 1}, "c");
    CHECK(!b.can_redo());
    CHECK_EQ(b.text(), "ac");
}

TEST(buffer_modified_tracks_saved_state) {
    Buffer b = make("x");
    CHECK(!b.is_modified());
    b.insert({0, 1}, "y");
    CHECK(b.is_modified());
    b.undo();
    CHECK(!b.is_modified()); // back at the saved state
    b.redo();
    b.mark_saved();
    CHECK(!b.is_modified());
    b.undo();
    CHECK(b.is_modified());
}

TEST(buffer_load_and_save_round_trip) {
    const auto path =
        std::filesystem::temp_directory_path() / "cursey_buffer_test.txt";
    {
        std::ofstream f(path, std::ios::binary);
        f << "line one\nline two\n";
    }
    Buffer b;
    std::string error;
    CHECK(b.load(path.string(), error));
    CHECK_EQ(b.line_count(), 2u);
    CHECK_EQ(b.line(1), "line two");

    b.insert({1, 8}, "!");
    CHECK(b.save(path.string(), error));
    CHECK(!b.is_modified());
    CHECK_EQ(read_file(path), "line one\nline two!\n");

    std::filesystem::remove(path);
}

TEST(buffer_keeps_crlf_line_endings) {
    const auto path =
        std::filesystem::temp_directory_path() / "cursey_crlf_test.txt";
    {
        std::ofstream f(path, std::ios::binary);
        f << "a\r\nb\r\n";
    }
    Buffer b;
    std::string error;
    CHECK(b.load(path.string(), error));
    CHECK_EQ(b.text(), "a\nb");
    CHECK(b.save(path.string(), error));
    CHECK_EQ(read_file(path), "a\r\nb\r\n");
    std::filesystem::remove(path);
}

TEST(buffer_load_missing_file_fails) {
    Buffer b;
    std::string error;
    CHECK(!b.load("/nonexistent/dir/file.txt", error));
    CHECK(!error.empty());
}
