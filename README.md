# File ML

[![CI](https://github.com/meta-legend/file-ml-library/actions/workflows/ci.yml/badge.svg)](https://github.com/meta-legend/file-ml-library/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A small, cross-platform C++17 library of filesystem utilities and a companion to
[Network ML](https://github.com/meta-legend/network-ml-library). It provides the
ML::File class. A friendly method-based
wrapper over <filesystem> and <fstream>: text and binary read/write, append,
copy/move, directory listing, recursive folder create/delete, and
exists/size/last-modified queries.

```cpp
#include "fileml.h"

ML::File f;
f.createFolders("data/cache");
f.createFile("data/cache/note.txt", "hello\n");
f.appendFile("data/cache/note.txt", "world\n");
std::string text = f.readFile("data/cache/note.txt");

// binary is just as easy
std::vector<unsigned char> bytes = { 0x00, 0xff, 0x10 };
f.writeBytes("data/cache/blob.bin", bytes);
auto back = f.readBytes("data/cache/blob.bin");
```

## API (ML::File)

- **create/write:** createFolder, createFolders, createFile, appendFile, writeBytes
- **atomic writes:** writeFileAtomic, writeBytesAtomic (write to a same-directory temp then rename; a crash mid-write leaves the target untouched)
- **read:** readFile (text, line-based), readAll (exact bytes), readBytes (raw)
- **line-based text:** readLines (splits on '\n', drops the empty tail, strips '\r' from CRLF), writeLines (joins with '\n' and adds a trailing newline)
- **delete/copy/move:** deleteFile, deleteFolder (recursive), copyFile, moveFile
- **listing:** listFiles (basenames, one level), listFilesRecursive (every file under a folder, returned as /-separated paths relative to it)
- **glob listing:** listFiles(folder, pattern) and listFilesRecursive(folder, pattern) with shell-style `*`, `?`, `[set]` matched against basenames
- **temp helpers:** makeTempFile(prefix="fileml"), makeTempDir(prefix="fileml") - create a fresh entry under the system temp dir and return its path
- **query/metadata:** exists, isFile, isDirectory, fileCharacterCount, fileSize (-1 if missing), lastModified (unix seconds, -1 if missing)

## Using it

> Different methods of using it below: 

### vcpkg registry

File ML is published through the same vcpkg registry as
[Network ML](https://github.com/meta-legend/meta-legend-vcpkg-registry).
Add the registry in a `vcpkg-configuration.json`, then:

```json
{ "dependencies": [ "file-ml" ] }
```

```cmake
find_package(FileML CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE FileML::fileml)
```

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(FileML
    GIT_REPOSITORY https://github.com/meta-legend/file-ml-library.git
    GIT_TAG v1.0.0)
FetchContent_MakeAvailable(FileML)
target_link_libraries(myapp PRIVATE FileML::fileml)
```

### Build from source

```sh
cmake -B build -S .
cmake --build build --config Release
cmake --install build --prefix /path/to/install
```

## License

MIT. See [LICENSE](LICENSE).
