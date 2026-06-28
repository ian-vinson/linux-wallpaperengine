#include "PackageParser.h"

#include "WallpaperEngine/Data/Assets/Package.h"
#include "WallpaperEngine/Logging/Log.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::Data::Utils;

// Highest pkg format version whose binary layout has been verified compatible.
static constexpr int PKG_MAX_KNOWN_VERSION = 24;

namespace {

uint32_t readU32LE (const char* p) {
    const auto* u = reinterpret_cast<const uint8_t*> (p);
    return uint32_t (u [0]) | (uint32_t (u [1]) << 8) | (uint32_t (u [2]) << 16) | (uint32_t (u [3]) << 24);
}

struct RawEntry {
    std::string filename;
    uint32_t offset; // absolute file offset (converted from relative for unpadded, verbatim for padded)
    uint32_t length;
};

// Unpadded format (all versions except PKGV0021-padded):
//   [uint32 nameLen][name bytes][uint32 relOff][uint32 size]
//   relOff is relative to the data section start.
//   Validation: dataStart + last.relOff + last.size == fileSize
bool tryUnpadded (const char* data, uint64_t fileSize, uint32_t entryCount,
                  uint32_t pos, std::vector<RawEntry>& out) {
    out.clear ();
    for (uint32_t i = 0; i < entryCount; ++i) {
        if (pos + 4 > fileSize) return false;
        const uint32_t nameLen = readU32LE (data + pos); pos += 4;
        if (nameLen == 0 || nameLen > 512) return false;
        if (pos + nameLen + 8 > fileSize) return false;
        std::string name (data + pos, nameLen); pos += nameLen;
        const uint32_t relOff = readU32LE (data + pos);
        const uint32_t size   = readU32LE (data + pos + 4);
        pos += 8;
        out.push_back ({std::move (name), relOff, size});
    }
    const uint32_t dataStart = pos;
    if (out.empty ()) return dataStart == fileSize;
    const auto& last = out.back ();
    if (uint64_t (dataStart) + last.offset + last.length != fileSize) return false;
    for (auto& e : out) e.offset += dataStart;
    return true;
}

// Padded format (PKGV0021-padded only):
//   [uint32 nameLen][name bytes][padding to 4-byte boundary][uint32 absOff][uint32 size]
//   absOff is absolute from file start.
bool tryPadded (const char* data, uint64_t fileSize, uint32_t entryCount,
                uint32_t pos, std::vector<RawEntry>& out) {
    out.clear ();
    for (uint32_t i = 0; i < entryCount; ++i) {
        if (pos + 4 > fileSize) return false;
        const uint32_t nameLen = readU32LE (data + pos); pos += 4;
        if (nameLen == 0 || nameLen > 512) return false;
        if (pos + nameLen > fileSize) return false;
        std::string name (data + pos, nameLen); pos += nameLen;
        const uint32_t pad = (4 - nameLen % 4) % 4;
        if (pos + pad + 8 > fileSize) return false;
        pos += pad;
        const uint32_t absOff = readU32LE (data + pos);
        const uint32_t size   = readU32LE (data + pos + 4);
        pos += 8;
        if (uint64_t (absOff) + size > fileSize) return false;
        out.push_back ({std::move (name), absOff, size});
    }
    return true;
}

struct PkgHeader {
    std::string version;
    uint32_t entryCount;
    uint32_t indexStart;
};

PkgHeader parseHeader (const char* data, uint64_t fileSize) {
    if (fileSize < 4)
        throw std::runtime_error ("PKG file too small");
    const uint32_t hdrLen = readU32LE (data);
    if (uint64_t (4) + hdrLen + 4 > fileSize)
        throw std::runtime_error ("PKG header truncated");
    std::string version (data + 4, hdrLen);
    if (!version.starts_with ("PKGV"))
        throw std::runtime_error ("Expected pkg header to start with PKGV, got: " + version);
    const uint32_t entryCount = readU32LE (data + 4 + hdrLen);
    return {std::move (version), entryCount, 4 + hdrLen + 4};
}

} // namespace

PackageUniquePtr PackageParser::parse (ReadStreamSharedPtr stream) {
    stream->seekg (0, std::ios::end);
    const uint64_t fileSize = static_cast<uint64_t> (stream->tellg ());
    stream->seekg (0, std::ios::beg);

    std::vector<char> buf (fileSize);
    stream->read (buf.data (), static_cast<std::streamsize> (fileSize));

    const char* data = buf.data ();
    const auto [version, entryCount, indexStart] = parseHeader (data, fileSize);

    int ver = 0;
    try {
        ver = std::stoi (version.substr (4));
    } catch (...) {
        throw std::runtime_error ("Cannot parse version number from pkg header: " + version);
    }

    if (ver > PKG_MAX_KNOWN_VERSION) {
        std::cerr << "WARNING: scene.pkg version " << version
                  << " is not fully supported. Some assets may be missing." << std::endl;
    }

    std::vector<RawEntry> rawEntries;
    bool usedPadded = false;

    if (!tryUnpadded (data, fileSize, entryCount, indexStart, rawEntries)) {
        rawEntries.clear ();
        if (!tryPadded (data, fileSize, entryCount, indexStart, rawEntries)) {
            throw std::runtime_error ("PKG parse failed for both formats: " + version);
        }
        usedPadded = true;
    }

    sLog.debug ("PKG ", version, ": ", entryCount, " entries, ", usedPadded ? "padded" : "unpadded", " format");

    // Reset stream for later seekg+read calls from PackageAdapter::open
    stream->seekg (0, std::ios::beg);

    auto reader = std::make_unique<BinaryReader> (std::move (stream));
    auto result = std::make_unique<Package> (Package {.file = std::move (reader)});
    result->baseOffset = 0; // entries store absolute file offsets

    result->files.reserve (rawEntries.size ());
    for (const auto& e : rawEntries) {
        result->files.push_back (
            std::make_unique<FileEntry> (FileEntry {.filename = e.filename, .offset = e.offset, .length = e.length}));
    }

    return result;
}

void PackageParser::dumpPkg (const std::filesystem::path& path) {
    std::ifstream file (path, std::ios::binary);
    if (!file.is_open ())
        throw std::runtime_error ("Cannot open PKG file: " + path.string ());

    file.seekg (0, std::ios::end);
    const uint64_t fileSize = static_cast<uint64_t> (file.tellg ());
    file.seekg (0, std::ios::beg);

    std::vector<char> buf (fileSize);
    file.read (buf.data (), static_cast<std::streamsize> (fileSize));

    const char* data = buf.data ();
    const auto [version, entryCount, indexStart] = parseHeader (data, fileSize);

    std::vector<RawEntry> rawEntries;
    bool usedPadded = false;
    bool ok = tryUnpadded (data, fileSize, entryCount, indexStart, rawEntries);
    if (!ok) {
        rawEntries.clear ();
        ok = tryPadded (data, fileSize, entryCount, indexStart, rawEntries);
        usedPadded = true;
    }

    std::cout << "PKG file:  " << path.string () << "\n"
              << "Version:   " << version << "\n"
              << "File size: " << fileSize << " bytes\n"
              << "Entries:   " << entryCount << "\n"
              << "Format:    " << (ok ? (usedPadded ? "padded" : "unpadded") : "PARSE FAILED") << "\n";

    if (ok) {
        std::cout << "\n";
        for (const auto& e : rawEntries) {
            std::cout << "  " << e.filename << "\toffset=" << e.offset << "\tsize=" << e.length << "\n";
        }
    }
}
