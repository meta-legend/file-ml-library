#pragma once
#include <string>
#include <vector>

// File ML - cross-platform filesystem utilities. A companion to Network ML,
// providing the ML::File class that used to ship with it. Header-light wrapper
// over <filesystem> and <fstream> with a friendly method-based API. No external
// dependencies.
namespace ML {

    class File {
    public:
        // create and write
        void createFolder(std::string name);                       // single level
        void createFolders(std::string path);                      // recursive (mkdir -p)
        void createFile(std::string name, std::string content);    // create / overwrite
        void appendFile(std::string name, std::string content);    // append to the end
        void writeBytes(std::string name, const std::vector<unsigned char>& data);

        // Atomic writes: write to a same-directory temp file, fsync, then rename
        // over the target. A crash mid-write leaves the original target untouched
        // (or absent if the target did not exist). Same-directory temp is required
        // so the rename is a real atomic rename, not a cross-filesystem copy+delete.
        void writeFileAtomic(std::string name, std::string content);
        void writeBytesAtomic(std::string name, const std::vector<unsigned char>& data);

        // read
        std::string readFile(std::string name);                    // text, line-based
        std::string readAll(std::string name);                     // exact bytes (binary-safe)
        std::vector<unsigned char> readBytes(std::string name);    // raw bytes

        // Line-based text convenience. readLines splits on '\n' and drops the empty
        // element that a trailing newline would produce, so a file ending with '\n'
        // does not add a phantom empty line. writeLines joins with '\n' and adds a
        // trailing newline so the file is well-formed for tools like `wc -l`.
        std::vector<std::string> readLines(std::string name);
        void writeLines(std::string name, const std::vector<std::string>& lines);

        // delete, copy, and move
        void deleteFile(std::string name);
        void deleteFolder(std::string name);                       // recursive
        void copyFile(std::string src, std::string dst);           // overwrites dst
        void moveFile(std::string src, std::string dst);           // rename/move

        // listing, queries, and metadata
        std::vector<std::string> listFiles(std::string folder);            // basenames, one level deep
        // Every file under folder, searched recursively. Returns paths relative to
        // folder with '/' separators (e.g. "images/birds/downflap.png"). Files only;
        // directories are not listed. Empty if folder is missing or unreadable.
        std::vector<std::string> listFilesRecursive(std::string folder);

        // Glob-filtered listings. The pattern is a shell-style glob matched against
        // the file's basename (never the whole relative path), so "*.png" matches
        // any PNG anywhere in the tree. Supported: * (any run of chars), ? (one
        // char), [set] (character class, [!set] to negate). Everything else is
        // literal, including path separators (basenames do not contain them).
        std::vector<std::string> listFiles(std::string folder, std::string pattern);
        std::vector<std::string> listFilesRecursive(std::string folder, std::string pattern);

        bool exists(std::string name);
        bool isFile(std::string name);
        bool isDirectory(std::string name);
        int fileCharacterCount(std::string name);
        long fileSize(std::string name);                           // bytes, -1 if missing
        long long lastModified(std::string name);                  // unix seconds, -1 if missing

        // Temp file and directory helpers. Both create a fresh entry under the
        // system temp directory (std::filesystem::temp_directory_path()) and
        // return its path. The prefix is prepended to a random suffix so the
        // path is easy to identify while remaining unique. Returns "" on
        // failure. The caller owns cleanup.
        std::string makeTempFile(std::string prefix = "fileml");
        std::string makeTempDir(std::string prefix = "fileml");
    };
}
