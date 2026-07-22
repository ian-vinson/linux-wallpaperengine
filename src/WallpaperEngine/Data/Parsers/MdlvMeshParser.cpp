#include "MdlvMeshParser.h"

#include <cstring>
#include <glm/glm.hpp>

#include "WallpaperEngine/Data/Utils/MemoryStream.h"

using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Utils;

std::vector<MdlvMeshBlock> WallpaperEngine::Data::Parsers::findMdlvMeshBlocks (
    const BinaryReader& reader, size_t markerSize, size_t endOffset, size_t meshHeaderSize, size_t vertexStride
) {
    std::vector<MdlvMeshBlock> blocks;
    size_t offset = markerSize;

    while (offset + meshHeaderSize + sizeof (uint32_t) < endOffset) {
	reader.base ().seekg (static_cast<std::streamoff> (offset + sizeof (uint32_t)), std::ios::beg);
	const uint32_t candidateVertexBytes = reader.nextUInt32 ();
	const size_t verticesOffset = offset + meshHeaderSize;
	const size_t indexLengthOffset = verticesOffset + candidateVertexBytes;

	if (candidateVertexBytes == 0 || candidateVertexBytes % vertexStride != 0
	    || indexLengthOffset + sizeof (uint32_t) > endOffset) {
	    offset++;
	    continue;
	}

	reader.base ().seekg (static_cast<std::streamoff> (indexLengthOffset), std::ios::beg);
	const uint32_t candidateIndexBytes = reader.nextUInt32 ();
	const size_t indicesOffset = indexLengthOffset + sizeof (uint32_t);

	if (candidateIndexBytes == 0 || candidateIndexBytes % (sizeof (uint16_t) * 3) != 0
	    || indicesOffset + candidateIndexBytes > endOffset) {
	    offset++;
	    continue;
	}

	blocks.push_back (
	    MdlvMeshBlock { .headerOffset = offset, .vertexBytes = candidateVertexBytes, .indexBytes = candidateIndexBytes }
	);
	// Jump past this block instead of just +1 -- consecutive blocks don't overlap, and a naive
	// +1 continuation risks re-matching a spurious "block" inside data we already consumed.
	offset = indicesOffset + candidateIndexBytes;
    }

    return blocks;
}

namespace {
// Per-block vertex layout, selected from the 4-byte flags word immediately preceding each mesh
// sub-block's vertex-length field (i.e. the first uint32 of the block header) -- confirmed via
// direct byte-level comparison across castle_courtyard.mdl/sky*.mdl (Ocarina of Time, 3737268876)
// and an unrelated built-in asset (particleelementpreviews/collisionmodel/sphere.mdl): every
// static (non-MDLS) sub-block's flags word is the constant 0x0000000f, while link_adult.mdl's
// skeletal/MDLS sub-blocks (already handled by CImage's puppet-warp path) carry 0x0180000f.
// Selecting per-block from this flags word, rather than trusting the file-level hasSkeleton check
// alone, keeps this robust if a stray skinned-flag block ever turns up in an otherwise static file.
enum class MdlvVertexLayout { Static, Skinned };

constexpr uint32_t kStaticVertexFlags = 0x0000000fu;
constexpr size_t kStaticVertexStride = 48; // position(12) + normal(12) + tangent(12) + tangent.w(4) + uv(8)
constexpr size_t kStaticNormalOffset = 12;
constexpr size_t kStaticUvOffset = 40;

// Skeletal/skinned blocks are out of scope for the static-mesh path (parseMdlv() already returns
// before ever reaching a block scan when hasSkeleton is set) -- this is a defensive fallback only,
// using the same stride/uv offset as CImage's confirmed-working puppet-warp loader. The stored
// normal offset for this 80-byte layout was never confirmed, so normals for any such block are
// still computed from face winding, exactly as this parser did before static meshes were handled.
constexpr size_t kSkinnedVertexStride = 80;
constexpr size_t kSkinnedUvOffset = 72;

struct MdlvAutoMeshBlock {
    size_t headerOffset = 0;
    uint32_t vertexBytes = 0;
    uint32_t indexBytes = 0;
    MdlvVertexLayout layout = MdlvVertexLayout::Static;
};

// Static-model-only sibling of findMdlvMeshBlocks: derives each candidate block's vertex stride
// from its own flags word instead of a single caller-supplied stride. Only used by parseMdlv() --
// CImage's puppet-mesh loader keeps calling the shared findMdlvMeshBlocks() directly with its own
// fixed stride (80), completely unchanged by this.
std::vector<MdlvAutoMeshBlock> findMdlvMeshBlocksAuto (
    const BinaryReader& reader, size_t markerSize, size_t endOffset, size_t meshHeaderSize
) {
    std::vector<MdlvAutoMeshBlock> blocks;
    size_t offset = markerSize;

    while (offset + meshHeaderSize + sizeof (uint32_t) < endOffset) {
	reader.base ().seekg (static_cast<std::streamoff> (offset), std::ios::beg);
	const uint32_t flags = reader.nextUInt32 ();
	const MdlvVertexLayout layout = flags == kStaticVertexFlags ? MdlvVertexLayout::Static : MdlvVertexLayout::Skinned;
	const size_t vertexStride = layout == MdlvVertexLayout::Static ? kStaticVertexStride : kSkinnedVertexStride;

	const uint32_t candidateVertexBytes = reader.nextUInt32 ();
	const size_t verticesOffset = offset + meshHeaderSize;
	const size_t indexLengthOffset = verticesOffset + candidateVertexBytes;

	if (candidateVertexBytes == 0 || candidateVertexBytes % vertexStride != 0
	    || indexLengthOffset + sizeof (uint32_t) > endOffset) {
	    offset++;
	    continue;
	}

	reader.base ().seekg (static_cast<std::streamoff> (indexLengthOffset), std::ios::beg);
	const uint32_t candidateIndexBytes = reader.nextUInt32 ();
	const size_t indicesOffset = indexLengthOffset + sizeof (uint32_t);

	if (candidateIndexBytes == 0 || candidateIndexBytes % (sizeof (uint16_t) * 3) != 0
	    || indicesOffset + candidateIndexBytes > endOffset) {
	    offset++;
	    continue;
	}

	blocks.push_back (MdlvAutoMeshBlock {
	    .headerOffset = offset, .vertexBytes = candidateVertexBytes, .indexBytes = candidateIndexBytes, .layout = layout
	});
	// Jump past this block instead of just +1, same reasoning as findMdlvMeshBlocks.
	offset = indicesOffset + candidateIndexBytes;
    }

    return blocks;
}

// Every embedded ".json" material-reference string in the file, paired with the byte offset
// where the string starts. Used to associate each mesh sub-block with whichever material path
// immediately precedes it -- empirically, each sub-block in a real .mdl file is preceded by
// exactly one such string.
std::vector<std::pair<size_t, std::string>> findMaterialPathStrings (const std::vector<char>& data, size_t markerSize, size_t endOffset) {
    std::vector<std::pair<size_t, std::string>> result;
    static const std::string suffix = ".json";

    for (size_t pos = markerSize; pos + suffix.size () <= endOffset; pos++) {
	if (std::memcmp (data.data () + pos, suffix.data (), suffix.size ()) != 0) {
	    continue;
	}

	const size_t end = pos + suffix.size ();
	size_t start = end;
	while (start > markerSize) {
	    const auto c = static_cast<unsigned char> (data[start - 1]);
	    if (c < 0x20 || c > 0x7E) {
		break;
	    }
	    start--;
	}

	if (end - start >= 10) {
	    result.emplace_back (start, std::string (data.data () + start, end - start));
	}
    }

    return result;
}

// Per-vertex smooth normals, accumulated from adjacent face windings. Used only for the skinned
// (80-byte stride) layout's defensive fallback path (see findMdlvMeshBlocksAuto) -- that layout's
// stored-normal byte offset was never confirmed against real files, unlike the static (48-byte
// stride) layout's confirmed offset 12. Computing normals directly from geometry sidesteps that
// uncertainty and is always correct for any mesh, regardless of layout.
std::vector<float> computeSmoothNormals (const std::vector<float>& positions, const std::vector<uint16_t>& indices) {
    const size_t vertexCount = positions.size () / 3;
    std::vector<float> normals (vertexCount * 3, 0.0f);

    for (size_t t = 0; t + 2 < indices.size (); t += 3) {
	const uint16_t i0 = indices[t];
	const uint16_t i1 = indices[t + 1];
	const uint16_t i2 = indices[t + 2];

	const glm::vec3 p0 (positions[i0 * 3], positions[i0 * 3 + 1], positions[i0 * 3 + 2]);
	const glm::vec3 p1 (positions[i1 * 3], positions[i1 * 3 + 1], positions[i1 * 3 + 2]);
	const glm::vec3 p2 (positions[i2 * 3], positions[i2 * 3 + 1], positions[i2 * 3 + 2]);

	const glm::vec3 faceNormal = glm::cross (p1 - p0, p2 - p0);

	for (const uint16_t idx : { i0, i1, i2 }) {
	    normals[idx * 3] += faceNormal.x;
	    normals[idx * 3 + 1] += faceNormal.y;
	    normals[idx * 3 + 2] += faceNormal.z;
	}
    }

    for (size_t i = 0; i < vertexCount; i++) {
	glm::vec3 n (normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
	const float len = glm::length (n);
	n = len > 1e-6f ? n / len : glm::vec3 (0.0f, 1.0f, 0.0f);
	normals[i * 3] = n.x;
	normals[i * 3 + 1] = n.y;
	normals[i * 3 + 2] = n.z;
    }

    return normals;
}
}

MdlvParseResult WallpaperEngine::Data::Parsers::parseMdlv (const std::vector<char>& data) {
    MdlvParseResult result;

    constexpr size_t markerSize = 9;
    constexpr size_t meshHeaderSize = sizeof (uint32_t) * 2;
    constexpr size_t positionOffset = 0;

    result.version = data.size () >= markerSize ? std::string (data.data (), 8) : "";

    size_t endOffset = data.size ();
    for (size_t offset = markerSize; offset + 4 <= data.size (); offset++) {
	if (std::memcmp (data.data () + offset, "MDLS", 4) == 0) {
	    endOffset = offset;
	    result.hasSkeleton = true;
	    break;
	}
    }

    if (result.hasSkeleton) {
	// Rigged/animated models are explicitly out of scope -- no point parsing mesh blocks the
	// caller isn't going to use.
	return result;
    }

    const auto materialPaths = findMaterialPathStrings (data, markerSize, endOffset);

    auto meshBuffer = std::make_unique<char[]> (data.size ());
    std::copy (data.begin (), data.end (), meshBuffer.get ());
    const BinaryReader reader (std::make_shared<MemoryStream> (std::move (meshBuffer), data.size ()));

    const auto blocks = findMdlvMeshBlocksAuto (reader, markerSize, endOffset, meshHeaderSize);

    for (const auto& block : blocks) {
	const bool isStatic = block.layout == MdlvVertexLayout::Static;
	const size_t vertexStride = isStatic ? kStaticVertexStride : kSkinnedVertexStride;
	const size_t uvOffset = isStatic ? kStaticUvOffset : kSkinnedUvOffset;

	const size_t vertexCount = block.vertexBytes / vertexStride;
	const size_t verticesOffset = block.headerOffset + meshHeaderSize;
	const size_t indicesOffset = verticesOffset + block.vertexBytes + sizeof (uint32_t);
	const size_t indexCount = block.indexBytes / sizeof (uint16_t);

	MdlvSubMesh sub;
	sub.positions.reserve (vertexCount * 3);
	sub.uvs.reserve (vertexCount * 2);
	if (isStatic) {
	    sub.normals.reserve (vertexCount * 3);
	}

	for (size_t index = 0; index < vertexCount; index++) {
	    const size_t vertexOffset = verticesOffset + index * vertexStride;
	    reader.base ().seekg (static_cast<std::streamoff> (vertexOffset + positionOffset), std::ios::beg);
	    const float x = reader.nextFloat ();
	    const float y = reader.nextFloat ();
	    const float z = reader.nextFloat ();

	    if (isStatic) {
		reader.base ().seekg (static_cast<std::streamoff> (vertexOffset + kStaticNormalOffset), std::ios::beg);
		sub.normals.push_back (reader.nextFloat ());
		sub.normals.push_back (reader.nextFloat ());
		sub.normals.push_back (reader.nextFloat ());
	    }

	    reader.base ().seekg (static_cast<std::streamoff> (vertexOffset + uvOffset), std::ios::beg);
	    const float u = reader.nextFloat ();
	    const float v = reader.nextFloat ();

	    sub.positions.push_back (x);
	    sub.positions.push_back (y);
	    sub.positions.push_back (z);
	    sub.uvs.push_back (u);
	    sub.uvs.push_back (v);
	}

	reader.base ().seekg (static_cast<std::streamoff> (indicesOffset), std::ios::beg);
	sub.indices.reserve (indexCount);
	bool validIndices = true;
	for (size_t index = 0; index < indexCount; index++) {
	    uint16_t value = 0;
	    reader.next (reinterpret_cast<char*> (&value), sizeof (value));
	    if (value >= vertexCount) {
		validIndices = false;
		break;
	    }
	    sub.indices.push_back (value);
	}

	if (!validIndices || sub.indices.empty ()) {
	    continue;
	}

	// Skinned-layout blocks (defensive fallback only, see findMdlvMeshBlocksAuto) have no
	// confirmed stored-normal offset -- compute from face winding, same as this parser did for
	// every block before static meshes were understood. Static-layout blocks already have real
	// stored normals populated above.
	if (!isStatic) {
	    sub.normals = computeSmoothNormals (sub.positions, sub.indices);
	}

	for (auto it = materialPaths.rbegin (); it != materialPaths.rend (); ++it) {
	    if (it->first < block.headerOffset) {
		sub.materialPath = it->second;
		break;
	    }
	}

	result.subMeshes.push_back (std::move (sub));
    }

    return result;
}
