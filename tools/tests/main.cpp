// Smoke tests for File ML, built from the in-tree source and driven by CTest.
// Fully offline and deterministic: exercises every File method, including a
// binary read/write roundtrip and the missing-file metadata sentinels.
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "fileml.h"

using namespace ML;

static int g_pass = 0;
static int g_fail = 0;

static void section(const std::string& name) {
    std::cout << "\n[" << name << "]\n";
}

#define CHECK(expr) do { \
    if (expr) { ++g_pass; std::cout << "  ok   " << #expr << "\n"; } \
    else      { ++g_fail; std::cout << "  FAIL " << #expr << "  @" << __LINE__ << "\n"; } \
} while (0)

#define CHECK_MSG(expr, msg) do { \
    if (expr) { ++g_pass; std::cout << "  ok   " << (msg) << "\n"; } \
    else      { ++g_fail; std::cout << "  FAIL " << (msg) << "  @" << __LINE__ << "\n"; } \
} while (0)

static void test_file() {
    section("File");
    File f;

    const std::string root = "smoke_tmp";
    const std::string sub  = root + "/nested/deep";
    const std::string t1   = root + "/hello.txt";
    const std::string t2   = root + "/copied.txt";
    const std::string t3   = root + "/moved.txt";
    const std::string bin  = root + "/bytes.bin";

    if (f.exists(root)) f.deleteFolder(root);

    f.createFolder(root);
    CHECK(f.isDirectory(root));

    f.createFolders(sub);
    CHECK(f.isDirectory(sub));

    f.createFile(t1, "line1\nline2\n");
    CHECK(f.exists(t1));
    CHECK(f.isFile(t1));
    CHECK(!f.isDirectory(t1));

    f.appendFile(t1, "line3\n");
    std::string text = f.readFile(t1);
    CHECK(text.find("line1") != std::string::npos);
    CHECK(text.find("line3") != std::string::npos);

    std::string raw = f.readAll(t1);
    CHECK(raw.size() >= text.size());

    CHECK(f.fileCharacterCount(t1) > 0);
    CHECK(f.fileSize(t1) > 0);
    CHECK(f.lastModified(t1) > 0);

    std::vector<unsigned char> data = { 0x00, 0xff, 0x10, 0x7f, 0x80, 0x01 };
    f.writeBytes(bin, data);
    std::vector<unsigned char> back = f.readBytes(bin);
    CHECK_MSG(back == data, "writeBytes/readBytes roundtrip");

    f.copyFile(t1, t2);
    CHECK(f.exists(t2));
    f.moveFile(t2, t3);
    CHECK(!f.exists(t2));
    CHECK(f.exists(t3));

    std::vector<std::string> entries = f.listFiles(root);
    CHECK(!entries.empty());

    // recursive listing: relative, forward-slashed, files-only, finds nested files
    f.createFile(sub + "/leaf.txt", "leaf\n");   // sub = root/nested/deep
    std::vector<std::string> all = f.listFilesRecursive(root);
    bool foundNested = false, foundTop = false, anyBackslash = false, anyDir = false;
    for (const auto& p : all) {
        if (p == "nested/deep/leaf.txt") foundNested = true;
        if (p == "hello.txt") foundTop = true;
        if (p.find('\\') != std::string::npos) anyBackslash = true;
        if (p == "nested" || p == "nested/deep") anyDir = true;   // dirs must not appear
    }
    CHECK_MSG(foundNested, "listFilesRecursive finds a deeply nested file by relative path");
    CHECK_MSG(foundTop, "listFilesRecursive includes top-level files");
    CHECK_MSG(!anyBackslash, "listFilesRecursive uses forward slashes");
    CHECK_MSG(!anyDir, "listFilesRecursive lists files only, not directories");
    CHECK(f.listFilesRecursive(root + "/does_not_exist").empty());

    CHECK(f.fileSize(root + "/does_not_exist") == -1);
    CHECK(f.lastModified(root + "/does_not_exist") == -1);

    f.deleteFile(t1);
    CHECK(!f.exists(t1));

    f.deleteFolder(root);
    CHECK(!f.exists(root));
}

static void test_atomic_writes() {
    section("Atomic writes");
    File f;
    const std::string root = "smoke_atomic";
    if (f.exists(root)) f.deleteFolder(root);
    f.createFolder(root);

    // Overwriting an existing file replaces its contents wholesale, not incrementally.
    const std::string p = root + "/config.json";
    f.createFile(p, "{\"old\":true}");
    f.writeFileAtomic(p, "{\"new\":true}");
    CHECK_MSG(f.readAll(p) == "{\"new\":true}", "writeFileAtomic overwrites content");

    // No .tmp staging files should linger after a successful write. If any do,
    // the rename step did not run or the temp path leaked out of the same folder.
    std::vector<std::string> after = f.listFiles(root);
    bool anyTmp = false;
    for (const auto& n : after) if (n.find(".tmp") != std::string::npos) anyTmp = true;
    CHECK_MSG(!anyTmp, "no .tmp files left behind after atomic write");

    // Binary path works too.
    const std::string bp = root + "/blob.bin";
    std::vector<unsigned char> payload = { 0xde, 0xad, 0xbe, 0xef, 0x00, 0xff };
    f.writeBytesAtomic(bp, payload);
    CHECK_MSG(f.readBytes(bp) == payload, "writeBytesAtomic roundtrip");

    // Creating a fresh file (target didn't exist) still works.
    const std::string fresh = root + "/fresh.txt";
    f.writeFileAtomic(fresh, "hello");
    CHECK(f.readAll(fresh) == "hello");

    f.deleteFolder(root);
}

static void test_lines() {
    section("Line-based text");
    File f;
    const std::string root = "smoke_lines";
    if (f.exists(root)) f.deleteFolder(root);
    f.createFolder(root);
    const std::string p = root + "/lines.txt";

    std::vector<std::string> in = { "alpha", "beta", "gamma" };
    f.writeLines(p, in);
    // writeLines adds a trailing newline; readLines drops the empty tail element.
    // Roundtrip should be lossless for lines that contain no CR or LF themselves.
    std::vector<std::string> out = f.readLines(p);
    CHECK_MSG(out == in, "writeLines/readLines roundtrip");

    // File actually has trailing newline on disk.
    std::string raw = f.readAll(p);
    CHECK_MSG(!raw.empty() && raw.back() == '\n', "writeLines ends the file with '\\n'");

    // CRLF is normalized on read (Windows-friendly).
    f.createFile(root + "/crlf.txt", "one\r\ntwo\r\nthree\r\n");
    std::vector<std::string> crlf = f.readLines(root + "/crlf.txt");
    CHECK_MSG(crlf.size() == 3, "readLines splits CRLF file correctly");
    CHECK_MSG(crlf.size() >= 1 && crlf[0] == "one", "readLines strips '\\r' from CRLF");

    // Empty file: no lines, no crash.
    f.createFile(root + "/empty.txt", "");
    CHECK(f.readLines(root + "/empty.txt").empty());

    f.deleteFolder(root);
}

static void test_glob() {
    section("Glob listing");
    File f;
    const std::string root = "smoke_glob";
    if (f.exists(root)) f.deleteFolder(root);
    f.createFolders(root + "/images/birds");
    f.createFolders(root + "/images/pipes");
    f.createFolders(root + "/audio");

    // one PNG at the top, some more nested, plus non-matching files
    f.createFile(root + "/logo.png", "");
    f.createFile(root + "/README.md", "");
    f.createFile(root + "/images/birds/red.png", "");
    f.createFile(root + "/images/birds/notes.txt", "");
    f.createFile(root + "/images/pipes/green.png", "");
    f.createFile(root + "/audio/flap.ogg", "");

    // one-level *.png at root: only logo.png
    auto top = f.listFiles(root, "*.png");
    CHECK_MSG(top.size() == 1 && top[0] == "logo.png", "listFiles(*.png) matches one file at root");

    // recursive *.png: three PNGs, files-only, forward-slashed relative paths
    auto pngs = f.listFilesRecursive(root, "*.png");
    CHECK_MSG(pngs.size() == 3, "recursive *.png finds all three PNGs");
    bool foundNestedPng = false, anyNonPng = false;
    for (const auto& p : pngs) {
        if (p == "images/birds/red.png") foundNestedPng = true;
        if (p.size() < 4 || p.substr(p.size() - 4) != ".png") anyNonPng = true;
    }
    CHECK_MSG(foundNestedPng, "recursive glob returns forward-slashed relative paths");
    CHECK_MSG(!anyNonPng, "recursive glob rejects non-matching files");

    // ? matches exactly one char
    f.createFile(root + "/ab.txt", "");
    f.createFile(root + "/abc.txt", "");
    auto two = f.listFiles(root, "??.txt");
    CHECK_MSG(two.size() == 1 && two[0] == "ab.txt", "? matches exactly one character");

    // character class [set]
    f.createFile(root + "/a1.log", "");
    f.createFile(root + "/a2.log", "");
    f.createFile(root + "/a9.log", "");
    auto cls = f.listFiles(root, "a[12].log");
    CHECK_MSG(cls.size() == 2, "[12] character class matches two files");

    // regex metacharacters in pattern must be treated as literals
    f.createFile(root + "/file.name.txt", "");
    auto dot = f.listFiles(root, "file.name.txt");
    CHECK_MSG(dot.size() == 1, "'.' is literal in glob, not regex any-char");

    // missing folder returns empty, no crash
    CHECK(f.listFiles(root + "/no_such_dir", "*").empty());
    CHECK(f.listFilesRecursive(root + "/no_such_dir", "*").empty());

    f.deleteFolder(root);
}

static void test_temp() {
    section("Temp helpers");
    File f;

    std::string a = f.makeTempFile();
    std::string b = f.makeTempFile();
    CHECK_MSG(!a.empty() && !b.empty(), "makeTempFile returns non-empty paths");
    CHECK_MSG(a != b, "consecutive temp files get distinct paths");
    CHECK_MSG(f.isFile(a) && f.isFile(b), "temp files exist on disk");

    std::string d = f.makeTempDir("smoke");
    CHECK_MSG(!d.empty() && f.isDirectory(d), "makeTempDir creates a directory");
    // prefix appears in the returned name
    CHECK_MSG(d.find("smoke") != std::string::npos, "makeTempDir honors the prefix");

    // basic usage: write, read back, then clean up like the caller owns it
    const std::string inside = d + "/data.txt";
    f.createFile(inside, "hi");
    CHECK(f.readAll(inside) == "hi");

    f.deleteFile(a);
    f.deleteFile(b);
    f.deleteFolder(d);
}

int main() {
    test_file();
    test_atomic_writes();
    test_lines();
    test_glob();
    test_temp();

    std::cout << "\n========================================\n";
    std::cout << " " << g_pass << " passed, " << g_fail << " failed\n";
    std::cout << "========================================\n";
    return g_fail == 0 ? 0 : 1;
}
