#include "sv/mesh_definition.hpp"
#include "sv/buffer.hpp"

#include <fstream>
#include <span>

#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>

#include <filesystem>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <ranges>
#include <type_traits>

namespace sv {

auto
convert_assimp_mesh(const aiMesh*, MeshData&, VertexOffset&, IndexOffset&)
  -> Mesh;

#define EXPECT_WRITE(stream, ptr, size)                                        \
  if (!stream.write(reinterpret_cast<const char*>(ptr), size))                 \
    return false;

#define EXPECT_READ_OFFSET(stream, ptr, offset, size)                          \
  if (!stream.read(reinterpret_cast<char*>(ptr) + offset, size))               \
    return false;
#define EXPECT_READ(stream, ptr, size) EXPECT_READ_OFFSET(stream, ptr, 0, size)

template<typename T>
struct Serializer
{
  static auto serialise(std::ostream&, const T&) -> bool = delete;
  static auto deserialise(std::istream&, T&) -> bool = delete;
};

template<>
struct Serializer<BoundingBox>
{
  static auto serialise(std::ostream& out, const BoundingBox& box) -> bool
  {
    EXPECT_WRITE(out, glm::value_ptr(box.minimum), sizeof(glm::vec3));
    EXPECT_WRITE(out, glm::value_ptr(box.maximum), sizeof(glm::vec3));
    return true;
  }
  static auto deserialise(std::istream& in, BoundingBox& out) -> bool
  {
    EXPECT_READ(in, &out, sizeof(glm::vec3));
    EXPECT_READ_OFFSET(in, &out, sizeof(glm::vec3), sizeof(glm::vec3));
    return true;
  }
};
template<class T>
concept trivially_serializable =
  std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

template<trivially_serializable T>
struct Serializer<T>
{
  static auto serialise(std::ostream& out, const T& val) -> bool
  {
    EXPECT_WRITE(out, &val, sizeof(T));
    return true;
  }
  static auto deserialise(std::istream& in, T& val) -> bool
  {
    EXPECT_READ_OFFSET(in, &val, 0, sizeof(T));
    return true;
  }
};

namespace {

template<class T>
concept has_custom_serializer =
  requires(std::istream& in, std::ostream& o, const T& t, T& out) {
    { Serializer<T>::serialise(o, t) } -> std::same_as<bool>;
    { Serializer<T>::deserialise(in, out) } -> std::same_as<bool>;
  };

inline auto
read_exact(std::istream& s, void* dst, std::size_t n) -> bool
{
  if (n >
      static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    s.setstate(std::ios::failbit);
    return false;
  }
  s.read(static_cast<char*>(dst), static_cast<std::streamsize>(n));
  return static_cast<bool>(s);
}

template<trivially_serializable T>
inline auto
read_pod(std::istream& s, T& out) -> bool
{
  return read_exact(s, &out, sizeof(T));
}

template<trivially_serializable T>
inline auto
read_vec(std::istream& s, std::vector<T>& out, std::size_t count) -> bool
{
  if (count == 0) {
    out.clear();
    return true;
  }
  if (count > (std::numeric_limits<std::size_t>::max() / sizeof(T))) {
    s.setstate(std::ios::failbit);
    return false;
  }
  out.resize(count);
  return read_exact(s, out.data(), count * sizeof(T));
}

inline auto
write_exact(std::ostream& s, const void* src, std::size_t n) -> std::ostream&
{
  s.write(static_cast<const char*>(src), static_cast<std::streamsize>(n));
  return s;
}

inline auto
write_u32(std::ostream& s, std::uint32_t v) -> std::ostream&
{
  return write_exact(s, &v, sizeof(v));
}

inline auto
write_u8(std::ostream& s, std::uint8_t v) -> std::ostream&
{
  return write_exact(s, &v, sizeof(v));
}

inline auto
operator>>(std::istream& s, MeshHeader& out) -> std::istream&
{
  MeshHeader tmp{};
  if (!read_pod(s, tmp.magic)) {
    s.setstate(std::ios::failbit);
    return s;
  }
  if (tmp.magic != magic_header) {
    s.setstate(std::ios::failbit);
    return s;
  }
  if (!read_pod(s, tmp.mesh_serial_version)) {
    s.setstate(std::ios::failbit);
    return s;
  }
  if (!read_pod(s, tmp.mesh_count)) {
    s.setstate(std::ios::failbit);
    return s;
  }
  if (!read_pod(s, tmp.index_data_size)) {
    s.setstate(std::ios::failbit);
    return s;
  }
  if (!read_pod(s, tmp.vertex_data_size)) {
    s.setstate(std::ios::failbit);
    return s;
  }
  out = tmp;
  return s;
}

inline auto
operator<<(std::ostream& out, const MeshHeader& h) -> std::ostream&
{
  write_u32(out, h.magic);
  write_u32(out, h.mesh_serial_version);
  write_u32(out, h.mesh_count);
  write_u32(out, h.index_data_size);
  write_u32(out, h.vertex_data_size);
  return out;
}

template<trivially_serializable T>
inline auto
operator<<(std::ostream& out, const std::vector<T>& in) -> std::ostream&
{
  const auto n = static_cast<std::uint32_t>(in.size());
  write_u32(out, n);
  if (n)
    write_exact(out, in.data(), static_cast<std::size_t>(n) * sizeof(T));
  if (!out)
    out.setstate(std::ios::failbit);
  return out;
}

template<trivially_serializable T>
inline auto
operator>>(std::istream& in, std::vector<T>& out) -> std::istream&
{
  std::uint32_t n{};
  if (!read_pod(in, n)) {
    in.setstate(std::ios::failbit);
    return in;
  }
  if (!read_vec(in, out, n)) {
    in.setstate(std::ios::failbit);
    return in;
  }
  return in;
}

template<typename T>
  requires(!trivially_serializable<T> && has_custom_serializer<T>)
inline auto
operator<<(std::ostream& out, const std::vector<T>& in) -> std::ostream&
{
  const auto n = static_cast<std::uint32_t>(in.size());
  write_u32(out, n);
  for (const auto& v : in) {
    if (!Serializer<T>::serialise(out, v)) {
      out.setstate(std::ios::failbit);
      break;
    }
  }
  return out;
}

template<typename T>
  requires(!trivially_serializable<T> && has_custom_serializer<T>)
inline auto
operator>>(std::istream& in, std::vector<T>& out) -> std::istream&
{
  std::uint32_t n{};
  if (!read_pod(in, n)) {
    in.setstate(std::ios::failbit);
    return in;
  }
  out.resize(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    if (!Serializer<T>::deserialise(in, out[i])) {
      in.setstate(std::ios::failbit);
      break;
    }
  }
  return in;
}

inline auto
operator<<(std::ostream& out, const VertexInput& in) -> std::ostream&
{
  const auto attr_count = in.get_attributes_count();
  write_u32(out, attr_count);
  for (std::uint32_t i = 0; i < attr_count; ++i) {
    const auto& a = in.attributes[i];
    write_u32(out, a.location);
    write_u32(out, a.binding);
    write_u32(out, static_cast<std::uint32_t>(a.format));
    write_u32(out, static_cast<std::uint32_t>(a.offset));
  }

  const auto binding_count = in.get_input_bindings_count();
  write_u32(out, binding_count);
  for (std::uint32_t i = 0; i < binding_count; ++i) {
    const auto& b = in.input_bindings[i];
    write_u32(out, b.stride);
    write_u8(out, static_cast<std::uint8_t>(b.rate));
    const std::uint8_t pad[3]{};
    write_exact(out, pad, sizeof(pad));
  }

  return out;
}

inline auto
operator>>(std::istream& in, VertexInput& out_vi) -> std::istream&
{
  std::uint32_t attr_count{};
  if (!read_pod(in, attr_count)) {
    in.setstate(std::ios::failbit);
    return in;
  }
  if (attr_count > VertexInput::vertex_attribute_max_count) {
    in.setstate(std::ios::failbit);
    return in;
  }

  for (std::uint32_t i = 0; i < attr_count; ++i) {
    VertexInput::VertexAttribute a{};
    if (!read_pod(in, a.location)) {
      in.setstate(std::ios::failbit);
      return in;
    }
    if (!read_pod(in, a.binding)) {
      in.setstate(std::ios::failbit);
      return in;
    }
    std::uint32_t fmt{};
    if (!read_pod(in, fmt)) {
      in.setstate(std::ios::failbit);
      return in;
    }
    a.format = static_cast<VertexFormat>(fmt);
    std::uint32_t off{};
    if (!read_pod(in, off)) {
      in.setstate(std::ios::failbit);
      return in;
    }
    a.offset = off;
    out_vi.attributes[i] = a;
  }

  std::uint32_t binding_count{};
  if (!read_pod(in, binding_count)) {
    in.setstate(std::ios::failbit);
    return in;
  }
  if (binding_count > VertexInput::input_bindings_max_count) {
    in.setstate(std::ios::failbit);
    return in;
  }

  for (std::uint32_t i = 0; i < binding_count; ++i) {
    VertexInput::VertexInputBinding b{};
    if (!read_pod(in, b.stride)) {
      in.setstate(std::ios::failbit);
      return in;
    }
    std::uint8_t rate_u8{};
    if (!read_exact(in, &rate_u8, sizeof(rate_u8))) {
      in.setstate(std::ios::failbit);
      return in;
    }
    b.rate = static_cast<VertexInput::VertexInputBinding::Rate>(rate_u8);
    std::uint8_t pad[3];
    if (!read_exact(in, pad, sizeof(pad))) {
      in.setstate(std::ios::failbit);
      return in;
    }
    out_vi.input_bindings[i] = b;
  }

  return in;
}

inline auto
append_bytes(std::vector<std::uint8_t>& dst, const void* p, std::size_t n)
  -> void
{
  const auto* b = static_cast<const std::uint8_t*>(p);
  dst.insert(dst.end(), b, b + n);
}

template<trivially_serializable T>
inline auto
append_bytes(std::vector<std::uint8_t>& dst, const T& t) -> void
{
  const auto* b =
    static_cast<const std::uint8_t*>(reinterpret_cast<const std::uint8_t*>(&t));
  dst.insert(dst.end(), b, b + sizeof(T));
}

inline auto
write_half4_from_texcoords(std::vector<std::uint8_t>& dst,
                           glm::vec2 uv0,
                           glm::vec2 uv1) -> void
{
  const std::uint32_t lo = glm::packHalf2x16(uv0);
  const std::uint32_t hi = glm::packHalf2x16(uv1);
  append_bytes(dst, &lo, sizeof(lo));
  append_bytes(dst, &hi, sizeof(hi));
}

template<typename T>
auto
merge_vectors(std::vector<T>& v1, const std::vector<T>& v2)
{
  v1.insert(v1.end(), v2.begin(), v2.end());
}

auto
recalculate_bounding_boxes(MeshData& m)
{
  const std::size_t stride =
    static_cast<std::size_t>(m.streams.compute_vertex_size());
  const std::span<const std::byte> vertices{
    reinterpret_cast<const std::byte*>(m.vertices.data()), m.vertices.size()
  };
  const std::span<const std::uint32_t> indices{ m.indices.data(),
                                                m.indices.size() };
  const std::span<const Mesh> meshes{ m.meshes.data(), m.meshes.size() };

  m.aabbs.clear();
  m.aabbs.resize(meshes.size());

  std::size_t mesh_i = 0;
  for (const auto& mesh : meshes) {
    const std::size_t num_indices =
      static_cast<std::size_t>(mesh.get_lod_index_count(0));
    const auto mesh_indices = indices.subspan(mesh.index_offset, num_indices);

    glm::vec3 vmin(std::numeric_limits<float>::max());
    glm::vec3 vmax(std::numeric_limits<float>::lowest());

    for (std::size_t i = 0; i < num_indices; ++i) {
      const std::size_t vertex_index =
        static_cast<std::size_t>(mesh_indices[i]) +
        static_cast<std::size_t>(mesh.vertex_offset);
      const auto* base = vertices.data() + vertex_index * stride;
      const auto* vf = reinterpret_cast<const float*>(base);
      const glm::vec3 v{ vf[0], vf[1], vf[2] };
      vmin = glm::min(vmin, v);
      vmax = glm::max(vmax, v);
    }

    m.aabbs[mesh_i++] = { vmin, vmax };
  }
}
} // namespace

template<>
struct Serializer<MeshData>
{
  static auto serialise(std::ostream& out, const MeshData& mesh) -> bool
  {
    out << mesh.streams;
    out << mesh.meshes;
    out << mesh.aabbs;
    out << mesh.vertices;
    out << mesh.indices;
    return static_cast<bool>(out);
  }
  static auto deserialise(std::istream& in, MeshData& mesh) -> bool
  {
    if (!(in >> mesh.streams))
      return false;
    if (!(in >> mesh.meshes))
      return false;
    if (!(in >> mesh.aabbs))
      return false;
    if (!(in >> mesh.vertices))
      return false;
    if (!(in >> mesh.indices))
      return false;
    return true;
  }
};

inline auto
operator>>(std::istream& in, MeshData& mesh) -> std::istream&
{
  if (!Serializer<MeshData>::deserialise(in, mesh))
    in.setstate(std::ios::failbit);
  return in;
}

inline auto
operator<<(std::ostream& out, const MeshData& mesh) -> std::ostream&
{
  if (!Serializer<MeshData>::serialise(out, mesh))
    out.setstate(std::ios::failbit);
  return out;
}

auto
save_mesh_file(const std::string_view path, const MeshFile& file) -> void
{
  std::ofstream stream{ path.data(), std::ios::binary };
  if (!stream)
    return;
  stream << file.header;
  stream << file.mesh;
}

auto
load_mesh_file(const std::string_view path) -> std::optional<MeshFile>
{
  std::ifstream stream{ path.data(), std::ios::binary };
  if (!stream)
    return std::nullopt;

  MeshFile file{};
  stream >> file.header;
  if (!stream)
    return std::nullopt;

  stream >> file.mesh;
  if (!stream)
    return std::nullopt;

  return file;
}

auto
save_mesh_data(const std::string_view path, const MeshData& mesh) -> bool
{
  if (std::filesystem::is_regular_file(path))
    return false;

  std::ofstream out{ path.data(), std::ios::binary | std::ios::out };
  if (!out)
    return false;

  const MeshHeader header{
    .mesh_count = static_cast<std::uint32_t>(mesh.meshes.size()),
    .index_data_size =
      static_cast<std::uint32_t>(std::span{ mesh.indices }.size_bytes()),
    .vertex_data_size =
      static_cast<std::uint32_t>(std::span{ mesh.vertices }.size_bytes()),
  };

  out << header;
  out << mesh;

  return static_cast<bool>(out);
}

auto
process_lods(std::vector<uint32_t>& indices,
             std::vector<uint8_t>& vertices,
             std::size_t vertex_stride,
             std::vector<std::vector<std::uint32_t>>& output_lods,
             bool should_generate_lods) -> void
{
  std::size_t vertex_count_in = vertices.size() / vertex_stride;
  std::size_t target_index_count = indices.size();

  output_lods.push_back(indices);

  if (!should_generate_lods)
    return;

  std::uint8_t LOD = 1;

  while (target_index_count > 1024 && LOD < max_lods) {
    target_index_count /= 2;

    bool sloppy = false;

    size_t num_opt_simplify = meshopt_simplify(indices.data(),
                                               indices.data(),
                                               (uint32_t)indices.size(),
                                               (const float*)vertices.data(),
                                               vertex_count_in,
                                               vertex_stride,
                                               target_index_count,
                                               0.02f,
                                               0,
                                               nullptr);

    if (static_cast<size_t>(num_opt_simplify * 1.1f) > indices.size()) {
      if (LOD > 1) {
        num_opt_simplify = meshopt_simplifySloppy(indices.data(),
                                                  indices.data(),
                                                  indices.size(),
                                                  (const float*)vertices.data(),
                                                  vertex_count_in,
                                                  vertex_stride,
                                                  target_index_count,
                                                  0.02f,
                                                  nullptr);
        sloppy = true;
        if (num_opt_simplify == indices.size())
          break;
      } else
        break;
    }

    indices.resize(num_opt_simplify);

    meshopt_optimizeVertexCache(
      indices.data(), indices.data(), indices.size(), vertex_count_in);
    LOD++;
    output_lods.push_back(indices);
  }
}

auto
convert_assimp_mesh(const aiMesh* ai_mesh,
                    MeshData& data,
                    VertexOffset& v,
                    IndexOffset& i) -> Mesh
{
  const auto count = ai_mesh->mNumVertices;
  auto empty_data = std::vector<aiVector3D>{};
  empty_data.resize(count);

  const auto positions = std::span{ ai_mesh->mVertices, count };
  const auto normals = ai_mesh->HasNormals()
                         ? std::span{ ai_mesh->mNormals, count }
                         : std::span{ empty_data };
  const auto tangents = ai_mesh->HasNormals()
                          ? std::span{ ai_mesh->mTangents, count }
                          : std::span{ empty_data };
  const auto bitangents = std::span{ ai_mesh->mBitangents, count };
  const auto tex_coords_0 = ai_mesh->HasTextureCoords(0)
                              ? std::span{ ai_mesh->mTextureCoords[0], count }
                              : std::span{ empty_data };
  const auto tex_coords_1 = ai_mesh->HasTextureCoords(1) ? std::span{
    ai_mesh->mTextureCoords[1],
    count,
  } : std::span{empty_data};

  std::vector<float> source_vertices;
  std::vector<std::uint32_t> source_indices;
  std::vector<std::uint8_t>& vertices = data.vertices;
  std::vector<std::vector<std::uint32_t>> output_lods;

  // Positions, TexCoord0,1 (HalfFloat2+HalfFloat2), Normal
  // (Int_2_10_10_10_REV), Tangent (Int_2_10_10_10_REV),
  // Bitangent(Int_2_10_10_10_REV)
  auto input = VertexInput::create({
    VertexFormat::Float3,
    VertexFormat::HalfFloat4,
    VertexFormat::Int_2_10_10_10_REV,
    VertexFormat::Int_2_10_10_10_REV,
    VertexFormat::Int_2_10_10_10_REV,
  });
  data.streams = input;

  for (auto&& [vertex, normal, tex_coord_0, tex_coord_1, tangent, bitangent] :
       std::ranges::views::zip(positions,
                               normals,
                               tex_coords_0,
                               tex_coords_1,
                               tangents,
                               bitangents)) {
    if (calculate_lods) {
      source_vertices.push_back(vertex.x);
      source_vertices.push_back(vertex.y);
      source_vertices.push_back(vertex.z);
    }

    append_bytes(vertices, vertex);
    write_half4_from_texcoords(data.vertices,
                               { tex_coord_0.x, tex_coord_0.y },
                               { tex_coord_1.x, tex_coord_1.y });
    auto packed_normals =
      glm::packSnorm3x10_1x2(glm::vec4{ normal.x, normal.y, normal.z, 0.0F });
    auto packed_tangents = glm::packSnorm3x10_1x2(
      glm::vec4{ tangent.x, tangent.y, tangent.z, 0.0F });
    auto packed_bitangents = glm::packSnorm3x10_1x2(
      glm::vec4{ bitangent.x, bitangent.y, bitangent.z, 0.0F });

    append_bytes(vertices, packed_normals);
    append_bytes(vertices, packed_tangents);
    append_bytes(vertices, packed_bitangents);
  }

  for (std::uint32_t face_index = 0; face_index < ai_mesh->mNumFaces;
       face_index++) {
    if (ai_mesh->mFaces[face_index].mNumIndices != 3)
      continue;
    for (std::uint32_t j = 0; j < ai_mesh->mFaces[face_index].mNumIndices; j++)
      source_indices.push_back(ai_mesh->mFaces[face_index].mIndices[j]);
  }

  const uint32_t vertex_stride = data.streams.compute_vertex_size();

  {
    const uint32_t vertex_count_prior =
      static_cast<std::uint32_t>(vertices.size()) / vertex_stride;
    std::vector<uint32_t> remap(vertex_count_prior);
    const size_t vertexCountOut =
      meshopt_generateVertexRemap(remap.data(),
                                  source_indices.data(),
                                  source_indices.size(),
                                  vertices.data(),
                                  vertex_count_prior,
                                  vertex_stride);

    std::vector<uint32_t> remapped_indices(source_indices.size());
    std::vector<uint8_t> remapped_vertices(vertexCountOut * vertex_stride);

    meshopt_remapIndexBuffer(remapped_indices.data(),
                             source_indices.data(),
                             source_indices.size(),
                             remap.data());
    meshopt_remapVertexBuffer(remapped_vertices.data(),
                              vertices.data(),
                              vertex_count_prior,
                              vertex_stride,
                              remap.data());

    meshopt_optimizeVertexCache(remapped_indices.data(),
                                remapped_indices.data(),
                                source_indices.size(),
                                vertexCountOut);
    meshopt_optimizeOverdraw(remapped_indices.data(),
                             remapped_indices.data(),
                             source_indices.size(),
                             (const float*)remapped_vertices.data(),
                             vertexCountOut,
                             vertex_stride,
                             1.05f);
    meshopt_optimizeVertexFetch(remapped_vertices.data(),
                                remapped_indices.data(),
                                source_indices.size(),
                                remapped_vertices.data(),
                                vertexCountOut,
                                vertex_stride);

    source_indices = remapped_indices;
    vertices = remapped_vertices;
  }

  const uint32_t numVertices =
    static_cast<uint32_t>(vertices.size() / vertex_stride);

  std::vector<std::vector<uint32_t>> out_lods;
  process_lods(
    source_indices, vertices, vertex_stride, out_lods, calculate_lods);

  Mesh result{
    .index_offset = static_cast<std::uint32_t>(i),
    .vertex_offset = static_cast<std::uint32_t>(v),
    .vertex_count = count,
  };

  std::uint32_t numIndices = 0;
  for (size_t l = 0; l < out_lods.size(); l++) {
    merge_vectors(data.indices, out_lods[l]);
    result.lod_offset[l] = numIndices;
    numIndices += (uint32_t)out_lods[l].size();
  }

  merge_vectors(data.vertices, vertices);

  result.lod_offset[out_lods.size()] = numIndices;
  result.lod_count = (uint32_t)out_lods.size();
  result.material_index = ai_mesh->mMaterialIndex;

  i += IndexOffset{ numIndices };
  v += VertexOffset{ numVertices };

  return result;
}

auto
load_mesh_data(const std::string_view path) -> std::optional<MeshData>
{
  constexpr std::uint32_t flags =
    aiProcess_JoinIdenticalVertices | aiProcess_Triangulate |
    aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights |
    aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
    aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates |
    aiProcess_FindInvalidData | aiProcess_GenUVCoords |
    aiProcess_CalcTangentSpace | aiProcess_EmbedTextures |
    aiProcess_MakeLeftHanded;

  Assimp::Importer importer;
  const aiScene* scene;
  if (scene = importer.ReadFile(path.data(), flags); nullptr == scene) {
    auto reason = importer.GetErrorString();
    std::cerr << reason << std::endl;
    return std::nullopt;
  }

  MeshData output;
  output.meshes.reserve(scene->mNumMeshes);
  output.aabbs.reserve(scene->mNumMeshes);

  VertexOffset vertex_offset{ 0 };
  IndexOffset index_offset{ 0 };
  for (std::uint32_t i = 0; i < scene->mNumMeshes; i++) {
    output.meshes.push_back(convert_assimp_mesh(
      scene->mMeshes[i], output, vertex_offset, index_offset));
    recalculate_bounding_boxes(output);
  }
  return output;
}

auto
RenderMesh::create(IContext& ctx, const std::string_view path)
  -> std::optional<RenderMesh>
{
  if (!std::filesystem::is_regular_file(path))
    return std::nullopt;

  RenderMesh mesh;
  if (auto maybe_file = load_mesh_file(path); maybe_file) {
    mesh.file = std::move(maybe_file.value());
  } else {
    return std::nullopt;
  }

  const auto filename = std::filesystem::path{ path }.filename();
  mesh.vertex_buffer = VulkanDeviceBuffer::create(
    ctx,
    {
      .data = as_bytes(std::span{ mesh.file.mesh.vertices }),
      .usage = BufferUsageBits::Vertex,
      .storage = StorageType::Device,
      .size = std::span{ mesh.file.mesh.vertices }.size_bytes(),
      .debug_name = std::format("{}_VB", filename.string()),
    });

  mesh.index_buffer = VulkanDeviceBuffer::create(
    ctx,
    {
      .data = as_bytes(std::span{ mesh.file.mesh.indices }),
      .usage = BufferUsageBits::Index,
      .storage = StorageType::Device,
      .size = std::span{ mesh.file.mesh.indices }.size_bytes(),
      .debug_name = std::format("{}_IB", filename.string()),
    });

  std::vector<std::uint8_t> draw_commands;
  const auto& command_count = mesh.file.header.mesh_count;
  draw_commands.resize(sizeof(VkDrawIndexedIndirectCommand) * command_count +
                       sizeof(std::uint32_t));
  std::memcpy(draw_commands.data(), &command_count, sizeof(command_count));
  VkDrawIndexedIndirectCommand* cmd =
    std::launder(reinterpret_cast<VkDrawIndexedIndirectCommand*>(
      draw_commands.data() + sizeof(std::uint32_t)));
  for (std::uint32_t i = 0; i < command_count; i++) {
    *cmd++ = VkDrawIndexedIndirectCommand{
      .indexCount = mesh.file.mesh.meshes[i].get_lod_index_count(0),
      .instanceCount = 1,
      .firstIndex = mesh.file.mesh.meshes[i].index_offset,
      .vertexOffset =
        static_cast<std::int32_t>(mesh.file.mesh.meshes[i].vertex_offset),
      .firstInstance = 0,
    };
  }

  mesh.indirect_buffer = VulkanDeviceBuffer::create(
    ctx,
    {
      .data = as_bytes(std::span{ draw_commands }),
      .usage = BufferUsageBits::Indirect,
      .storage = StorageType::Device,
      .size = std::span{ draw_commands }.size_bytes(),
      .debug_name = std::format("{}_IndirectBuffer", filename.string()),
    });

  const auto transforms =
    generate_n(1, [](const auto&) { return glm::identity<glm::mat4>(); });
  mesh.transform_buffer = VulkanDeviceBuffer::create(
    ctx,
    {
      .data = as_bytes(std::span{ draw_commands }),
      .usage = BufferUsageBits::Storage,
      .storage = StorageType::Device,
      .size = std::span{ draw_commands }.size_bytes(),
      .debug_name = std::format("{}_TransformBuffer", filename.string()),
    });

  struct SomeMaterial
  {
    std::uint32_t texture;
  };
  const auto materials =
    generate_n(mesh.file.mesh.meshes.size(),
               [](const auto) -> SomeMaterial { return { 0 }; });
  mesh.material_buffer = VulkanDeviceBuffer::create(
    ctx,
    {
      .data = as_bytes(std::span{ materials }),
      .usage = BufferUsageBits::Storage,
      .storage = StorageType::Device,
      .size = std::span{ materials }.size_bytes(),
      .debug_name = std::format("{}_MaterialBuffer", filename.string()),
    });

  return mesh;
}

}
