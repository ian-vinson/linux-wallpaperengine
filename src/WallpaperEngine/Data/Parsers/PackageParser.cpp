#include "PackageParser.h"

#include "WallpaperEngine/Data/Utils/BinaryReader.h"
#include "WallpaperEngine/Logging/Log.h"

#include "WallpaperEngine/Data/Assets/Package.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::Data::Utils;

// Highest pkg format version whose binary layout has been verified compatible.
// The index structure (sized-string header, uint32 count, per-entry sized-string name + uint32
// offset + uint32 length) has been stable from PKGV0001 through PKGV0024.
static constexpr int PKG_MAX_KNOWN_VERSION = 24;

PackageUniquePtr PackageParser::parse (ReadStreamSharedPtr stream) {
    auto reader = std::make_unique<BinaryReader> (std::move (stream));

    const std::string header = reader->nextSizedString ();

    if (!header.starts_with ("PKGV")) {
	throw std::runtime_error ("Expected pkg header to start with PKGV, got: " + header);
    }

    int version = 0;
    try {
	version = std::stoi (header.substr (4));
    } catch (...) {
	throw std::runtime_error ("Cannot parse version number from pkg header: " + header);
    }

    if (version > PKG_MAX_KNOWN_VERSION) {
	std::cerr << "WARNING: scene.pkg version " << header
		  << " is not fully supported. Some assets may be missing." << std::endl;
    }

    auto result = std::make_unique<Package> (Package {
	.file = std::move (reader),
    });

    result->files = parseFileList (*result->file);
    result->baseOffset = result->file->base ().tellg ();

    return result;
}

FileEntryList PackageParser::parseFileList (const BinaryReader& stream) {
    FileEntryList result = {};
    const uint32_t filesCount = stream.nextUInt32 ();

    result.reserve (filesCount);

    for (uint32_t i = 0; i < filesCount; i++) {
	result.push_back (
	    std::make_unique<FileEntry> (FileEntry {
		.filename = stream.nextSizedString (), .offset = stream.nextUInt32 (), .length = stream.nextUInt32 () })
	);
    }

    return result;
}
