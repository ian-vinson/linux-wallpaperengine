#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "WallpaperEngine/Data/Utils/BinaryReader.h"

namespace WallpaperEngine::Data::Parsers {
using namespace WallpaperEngine::Data::Utils;

struct MdlvMeshBlock {
    size_t headerOffset = 0;
    uint32_t vertexBytes = 0;
    uint32_t indexBytes = 0;
};

// Locates every mesh sub-block (a 4-byte vertex-buffer-length prefix + vertex data + a 4-byte
// index-buffer-length prefix + index data) between markerSize and endOffset in an MDLV file, at a
// single caller-supplied vertex stride. Used directly by CImage's puppet-warp mesh loader (which
// only ever consumes the first block found, fixed at stride 80 -- confirmed correct for skeletal/
// puppet files, unchanged). ObjectParser's static "model"-keyed 3D object loader does NOT use this
// function directly (see findMdlvMeshBlocksAuto in the .cpp, file-local) -- static (non-MDLS) MDLV
// files use a different, confirmed 48-byte vertex stride with real stored normal/tangent/uv
// fields, not the 80-byte skeletal layout.
std::vector<MdlvMeshBlock> findMdlvMeshBlocks (
    const BinaryReader& reader, size_t markerSize, size_t endOffset, size_t meshHeaderSize, size_t vertexStride
);

struct MdlvSubMesh {
    std::vector<float> positions; // 3 floats per vertex
    std::vector<float> normals; // 3 floats per vertex -- read directly from the confirmed offset
                                 // (12) in a static-layout (48-byte stride) block; for a skinned-
                                 // layout (80-byte stride) block encountered in a non-MDLS file
                                 // (not observed in practice, but handled defensively) the stored
                                 // normal offset is unconfirmed, so these are instead computed from
                                 // face winding, same as this parser's original behavior
    std::vector<float> uvs; // 2 floats per vertex
    std::vector<uint16_t> indices;
    std::optional<std::string> materialPath; // nearest preceding embedded ".json" path, if any
};

struct MdlvParseResult {
    std::string version;
    bool hasSkeleton = false; // "MDLS" marker found -- rigged/animated model, out of scope here
    std::vector<MdlvSubMesh> subMeshes;
};

// Full static-mesh parse: version check, MDLS-presence detection, every mesh sub-block up to the
// skeleton section (or EOF if none), each correlated with whichever embedded material path string
// immediately precedes it. Used by ObjectParser::parseModel3D() both to decide routing (skeleton
// present -> existing typeless-placeholder path, unchanged) and, when no skeleton, to build real
// CModel geometry.
MdlvParseResult parseMdlv (const std::vector<char>& data);
}
