# LZMA SDK (vendored)

These files are Igor Pavlov's LZMA SDK, **version 18.05 (2018-04-30)**,
taken unmodified from the SDK's `C/` directory. They are what
`src/common/sevenzip.c` reads `.7z` archives with.

> `MY_COPYRIGHT_PD "Igor Pavlov : Public domain"` — `7zVersion.h`

The SDK's C code is released into the public domain, which is compatible
with this project's GPL-2.0. Nothing here has been edited: the subset is
chosen by what a read-only 7z reader needs, and keeping the files byte for
byte as they ship is what makes updating to a later SDK a copy rather than
a merge.

## What is here, and why

| files | why |
|---|---|
| `7z.h`, `7zArcIn.c`, `7zDec.c`, `7zBuf.c`, `7zStream.c`, `7zFile.c`, `7zAlloc.c` | reading a 7z's header and decoding an entry |
| `7zCrc.c`, `7zCrcOpt.c` | the checksum the format stores per entry |
| `LzmaDec.c`, `Lzma2Dec.c` | the two compression methods essentially every 7z uses |
| `Ppmd7.c`, `Ppmd7Dec.c` | the third one, used for text-heavy archives |
| `Bcj2.c`, `Bra.c`, `Bra86.c`, `BraIA64.c`, `Delta.c` | the filters an archive may have been written through |
| `CpuArch.c`, `Compiler.h`, `Precomp.h`, `7zTypes.h` | what the above is built on |

Compression is not included: the launcher only ever reads archives.

## Building

The build compiles these with warnings off (`-w`), in
`src/onsjh_vitagui/CMakeLists.txt` and in `test/run_tests.sh`. They are not
this project's code to tidy, and their warnings would bury ours.

## Updating

Replace the files from a later SDK's `C/` directory, keeping the same list,
and run `sh test/run_tests.sh` — `test/test_archive.c` decodes real
fixtures and will say if the API moved.
