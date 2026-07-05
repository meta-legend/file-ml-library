#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
#include <filesystem>
#include <system_error>
#include <chrono>
#include <random>
#include <regex>
#include "fileml.h"

namespace ML {

    namespace {
        // Random hex suffix for temp names and atomic-write staging files. Each
        // thread gets its own generator, seeded once from std::random_device.
        std::string uniqueSuffix() {
            static thread_local std::mt19937_64 rng{std::random_device{}()};
            std::ostringstream oss;
            oss << std::hex << rng();
            return oss.str();
        }

        // Translate a shell-style glob into a regex. * -> .*, ? -> ., [set] passes
        // through with a leading ! flipped to ^, and regex metacharacters that
        // aren't part of glob syntax are escaped. Anchored to match the whole
        // basename, not a substring.
        std::regex globToRegex(const std::string& pattern) {
            std::string r;
            r.reserve(pattern.size() * 2 + 2);
            r.push_back('^');
            bool inClass = false;
            for (size_t i = 0; i < pattern.size(); ++i) {
                const char c = pattern[i];
                if (inClass) {
                    if (c == ']') { r.push_back(']'); inClass = false; }
                    else          { r.push_back(c); }
                    continue;
                }
                switch (c) {
                    case '*': r += ".*"; break;
                    case '?': r += '.'; break;
                    case '[':
                        r.push_back('[');
                        if (i + 1 < pattern.size() && pattern[i + 1] == '!') {
                            r.push_back('^');
                            ++i;
                        }
                        inClass = true;
                        break;
                    // regex metacharacters that are literal in glob
                    case '.': case '\\': case '+': case '(': case ')':
                    case '^': case '$': case '{': case '}': case '|':
                        r.push_back('\\'); r.push_back(c); break;
                    default:
                        r.push_back(c);
                }
            }
            r.push_back('$');
            return std::regex(r);
        }
    }

    // create and write

    void File::createFolder(std::string name) {
        std::error_code ec;
        if (!std::filesystem::create_directory(name, ec)) {
            if (ec) std::cerr << "Error creating folder: " << ec.message() << '\n';
            else    std::cerr << "Error creating folder: Folder already exists" << '\n';
        }
    }

    void File::createFolders(std::string path) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) std::cerr << "Error creating folders: " << ec.message() << '\n';
    }

    void File::createFile(std::string name, std::string content) {
        std::ofstream file(name, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error creating file: cannot open " << name << '\n';
            return;
        }
        file << content;
    }

    void File::appendFile(std::string name, std::string content) {
        std::ofstream file(name, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            std::cerr << "Error appending file: cannot open " << name << '\n';
            return;
        }
        file << content;
    }

    void File::writeBytes(std::string name, const std::vector<unsigned char>& data) {
        std::ofstream file(name, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error writing file: cannot open " << name << '\n';
            return;
        }
        file.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    }

    // Atomic writes: stage to <name>.<random>.tmp in the same directory, flush,
    // close, then rename over the target. std::filesystem::rename is atomic on
    // POSIX when both paths are on the same filesystem, and on Windows uses
    // MoveFileExW with REPLACE_EXISTING semantics, which is close enough that a
    // partial write cannot be observed at the target path.
    void File::writeFileAtomic(std::string name, std::string content) {
        std::filesystem::path target(name);
        std::filesystem::path tmp = target;
        tmp += "." + uniqueSuffix() + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary);
            if (!out.is_open()) {
                std::cerr << "Error atomic write: cannot open " << tmp.string() << '\n';
                return;
            }
            out << content;
            out.flush();
            if (!out) {
                std::cerr << "Error atomic write: stream failure on " << tmp.string() << '\n';
                std::error_code rec; std::filesystem::remove(tmp, rec);
                return;
            }
        }
        std::error_code ec;
        std::filesystem::rename(tmp, target, ec);
        if (ec) {
            std::cerr << "Error atomic write: rename failed: " << ec.message() << '\n';
            std::error_code rec; std::filesystem::remove(tmp, rec);
        }
    }

    void File::writeBytesAtomic(std::string name, const std::vector<unsigned char>& data) {
        std::filesystem::path target(name);
        std::filesystem::path tmp = target;
        tmp += "." + uniqueSuffix() + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary);
            if (!out.is_open()) {
                std::cerr << "Error atomic write: cannot open " << tmp.string() << '\n';
                return;
            }
            out.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
            out.flush();
            if (!out) {
                std::cerr << "Error atomic write: stream failure on " << tmp.string() << '\n';
                std::error_code rec; std::filesystem::remove(tmp, rec);
                return;
            }
        }
        std::error_code ec;
        std::filesystem::rename(tmp, target, ec);
        if (ec) {
            std::cerr << "Error atomic write: rename failed: " << ec.message() << '\n';
            std::error_code rec; std::filesystem::remove(tmp, rec);
        }
    }

    // read

    std::string File::readFile(std::string name) {
        std::ifstream inFile(name);
        if (!inFile.is_open()) {
            std::cerr << "Error reading file: cannot open " << name << '\n';
            return "Unable to open file";
        }
        std::string fileContent, line;
        while (inFile >> std::ws && std::getline(inFile, line)) {
            fileContent += line;
            fileContent.push_back('\n');
        }
        return fileContent;
    }

    std::string File::readAll(std::string name) {
        std::ifstream inFile(name, std::ios::binary);
        if (!inFile.is_open()) {
            std::cerr << "Error reading file: cannot open " << name << '\n';
            return "";
        }
        std::ostringstream ss;
        ss << inFile.rdbuf();   // exact bytes (no whitespace mangling)
        return ss.str();
    }

    std::vector<unsigned char> File::readBytes(std::string name) {
        std::ifstream inFile(name, std::ios::binary);
        if (!inFile.is_open()) {
            std::cerr << "Error reading file: cannot open " << name << '\n';
            return {};
        }
        return std::vector<unsigned char>(
            (std::istreambuf_iterator<char>(inFile)),
            std::istreambuf_iterator<char>());
    }

    std::vector<std::string> File::readLines(std::string name) {
        std::vector<std::string> lines;
        std::ifstream inFile(name);
        if (!inFile.is_open()) {
            std::cerr << "Error reading file: cannot open " << name << '\n';
            return lines;
        }
        std::string line;
        while (std::getline(inFile, line)) {
            // strip a stray '\r' from CRLF files so callers see clean lines
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(std::move(line));
        }
        return lines;
    }

    void File::writeLines(std::string name, const std::vector<std::string>& lines) {
        std::ofstream out(name, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "Error writing file: cannot open " << name << '\n';
            return;
        }
        for (const auto& line : lines) {
            out << line << '\n';
        }
    }

    // delete, copy, and move

    void File::deleteFile(std::string name) {
        std::error_code ec;
        if (!std::filesystem::remove(name, ec)) {
            if (ec) std::cerr << "Error deleting file: " << ec.message() << '\n';
            else    std::cerr << "Error deleting file: File does not exist" << '\n';
        }
    }

    void File::deleteFolder(std::string name) {
        std::error_code ec;
        std::filesystem::remove_all(name, ec);
        if (ec) std::cerr << "Error deleting folder: " << ec.message() << '\n';
    }

    void File::copyFile(std::string src, std::string dst) {
        std::error_code ec;
        std::filesystem::copy_file(src, dst,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) std::cerr << "Error copying file: " << ec.message() << '\n';
    }

    void File::moveFile(std::string src, std::string dst) {
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        if (ec) std::cerr << "Error moving file: " << ec.message() << '\n';
    }

    // listing, queries, and metadata

    std::vector<std::string> File::listFiles(std::string folder) {
        std::vector<std::string> entries;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
            entries.push_back(entry.path().filename().string());
        }
        if (ec) std::cerr << "Error listing folder: " << ec.message() << '\n';
        return entries;
    }

    std::vector<std::string> File::listFilesRecursive(std::string folder) {
        std::vector<std::string> entries;
        std::error_code ec;
        const std::filesystem::path base(folder);
        auto it = std::filesystem::recursive_directory_iterator(
            base, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        if (ec) {
            std::cerr << "Error walking folder: " << ec.message() << '\n';
            return entries;
        }
        for (; it != end; it.increment(ec)) {
            if (ec) {
                std::cerr << "Error walking folder: " << ec.message() << '\n';
                break;
            }
            std::error_code fec;
            if (std::filesystem::is_regular_file(it->path(), fec)) {
                // path relative to base, always forward-slashed for cross-platform keys
                entries.push_back(
                    std::filesystem::relative(it->path(), base, fec).generic_string());
            }
        }
        return entries;
    }

    std::vector<std::string> File::listFiles(std::string folder, std::string pattern) {
        std::vector<std::string> entries;
        std::regex re;
        try { re = globToRegex(pattern); }
        catch (const std::regex_error& e) {
            std::cerr << "Error listing folder: bad glob pattern: " << e.what() << '\n';
            return entries;
        }
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
            const std::string name = entry.path().filename().string();
            if (std::regex_match(name, re)) entries.push_back(name);
        }
        if (ec) std::cerr << "Error listing folder: " << ec.message() << '\n';
        return entries;
    }

    std::vector<std::string> File::listFilesRecursive(std::string folder, std::string pattern) {
        std::vector<std::string> entries;
        std::regex re;
        try { re = globToRegex(pattern); }
        catch (const std::regex_error& e) {
            std::cerr << "Error walking folder: bad glob pattern: " << e.what() << '\n';
            return entries;
        }
        std::error_code ec;
        const std::filesystem::path base(folder);
        auto it = std::filesystem::recursive_directory_iterator(
            base, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        if (ec) {
            std::cerr << "Error walking folder: " << ec.message() << '\n';
            return entries;
        }
        for (; it != end; it.increment(ec)) {
            if (ec) {
                std::cerr << "Error walking folder: " << ec.message() << '\n';
                break;
            }
            std::error_code fec;
            if (std::filesystem::is_regular_file(it->path(), fec)
                && std::regex_match(it->path().filename().string(), re)) {
                entries.push_back(
                    std::filesystem::relative(it->path(), base, fec).generic_string());
            }
        }
        return entries;
    }

    bool File::exists(std::string name) {
        std::error_code ec;
        return std::filesystem::exists(name, ec);
    }

    bool File::isFile(std::string name) {
        std::error_code ec;
        return std::filesystem::is_regular_file(name, ec);
    }

    bool File::isDirectory(std::string name) {
        std::error_code ec;
        return std::filesystem::is_directory(name, ec);
    }

    int File::fileCharacterCount(std::string name) {
        return static_cast<int>(readAll(name).length());
    }

    long File::fileSize(std::string name) {
        std::error_code ec;
        auto size = std::filesystem::file_size(name, ec);
        if (ec) return -1;
        return static_cast<long>(size);
    }

    long long File::lastModified(std::string name) {
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(name, ec);
        if (ec) return -1;
        // Convert the filesystem clock to system_clock
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now()
                  + std::chrono::system_clock::now());
        return static_cast<long long>(std::chrono::system_clock::to_time_t(sctp));
    }

    // Temp helpers. We generate a random hex suffix and retry a small number of
    // times to handle the vanishingly unlikely collision, so the returned path
    // is guaranteed fresh at creation time.
    std::string File::makeTempFile(std::string prefix) {
        std::error_code ec;
        const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
        if (ec) {
            std::cerr << "Error temp file: cannot get temp dir: " << ec.message() << '\n';
            return "";
        }
        for (int attempt = 0; attempt < 32; ++attempt) {
            std::filesystem::path candidate = base / (prefix + "-" + uniqueSuffix() + ".tmp");
            std::error_code eec;
            if (std::filesystem::exists(candidate, eec)) continue;
            std::ofstream out(candidate, std::ios::binary);
            if (!out.is_open()) continue;
            out.close();
            return candidate.string();
        }
        std::cerr << "Error temp file: exhausted retries picking a unique name\n";
        return "";
    }

    std::string File::makeTempDir(std::string prefix) {
        std::error_code ec;
        const std::filesystem::path base = std::filesystem::temp_directory_path(ec);
        if (ec) {
            std::cerr << "Error temp dir: cannot get temp dir: " << ec.message() << '\n';
            return "";
        }
        for (int attempt = 0; attempt < 32; ++attempt) {
            std::filesystem::path candidate = base / (prefix + "-" + uniqueSuffix());
            std::error_code cec;
            if (std::filesystem::create_directory(candidate, cec)) {
                return candidate.string();
            }
            // create_directory returns false without ec if it already existed; retry.
        }
        std::cerr << "Error temp dir: exhausted retries picking a unique name\n";
        return "";
    }
}
