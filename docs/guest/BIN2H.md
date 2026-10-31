# bin2h

bin2h embeds a binary file as a C byte array. It is a standalone host tool for
embedding BOOTSPUR images, ROAP files or other binary assets. It does not parse
or validate their contents.

## Building

Build from `fuselage/source/MISL/tools` with GNU Make:

```sh
make
```

The default compiler is Clang. `CC`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, `LDLIBS`
and `BUILD_DIR` can be overridden. The default output directory is
`fuselage/build/tools`. No engine libraries, RISC-V compiler, Python or CMake
are needed. The implementation uses C11 and native Windows or POSIX file APIs.
For a direct build:

```sh
clang -std=c11 -O2 bin2h.c -o bin2h
```

Use `bin2h.exe` as the output name on Windows. A checkout path may contain spaces;
run Make from the tool directory and use a relative `BUILD_DIR` without spaces.
`make clean` removes only this tool's executable from the selected directory.

## Command

```sh
bin2h input.bin output.h array_name
```

Exactly three arguments are required. Quote paths containing spaces. Exit status
is zero on success and nonzero on an argument or file error. Diagnostics go to
standard error.

The array name must start with an ASCII letter and contain only ASCII letters,
digits and underscores. Keywords, leading underscores and double underscores
are rejected for C/C++ compatibility. Choose a name that does not conflict with
the including program's declarations or macros.

## Output and failure handling

The header declares `static const unsigned char array_name[]` and
`static const unsigned int array_name_len`. The length is the input byte count,
bounded by the host's `UINT_MAX`. The tool streams bytes without retaining the
entire input in memory. An empty input emits one zero byte as array storage and
a length of zero; that placeholder is not input data.

The generated comment contains no source path, timestamp or machine details.
Identical input bytes and array names produce identical headers, including LF
line endings. Nonempty arrays retain the original tool's declarations and
twelve-byte row layout.

The tool rejects input and output paths identifying the same existing file,
including hard-link aliases. It creates an exclusive staging file beside the
destination, checks reads, writes and closes, then replaces the destination.
Errors before replacement leave an existing output intact. Only staging files
created by this invocation are removed; pre-existing staging names are skipped.
A failed cleanup reports the remaining staging filename.

Competing writers use separate staging files. A successful replacement installs
a complete header; Windows can reject a competing replacement, which is reported
as a failure. Build rules should give each output one producer. Abrupt termination
can leave a staging file, and this mechanism does not promise power-loss recovery.
Avoid modifying the input or output concurrently with generation.

## BOOTSPUR use

For a system-ROM image, link the RISC-V assembly before extracting its text and
running bin2h. See [System-ROM image](MISL_ROM.md#embedded-image). The tool does
not resolve relocations, check a ROM version or choose its entry-table layout.
Native engine builds consume the checked-in image header; they do not need to
regenerate it to use bin2h's output.
