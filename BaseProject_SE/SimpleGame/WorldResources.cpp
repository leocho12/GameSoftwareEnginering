#include "stdafx.h"
#include "WorldResources.h"
#include "RpgGame.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <fstream>
#include <locale>
#include <stdexcept>

namespace
{
constexpr int ChunkSize = 12;
constexpr float Pi = 3.14159265358979323846f;

float Milliseconds(std::chrono::steady_clock::time_point started)
{
    return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started)
        .count();
}

template <class T> void ReserveAdditional(std::vector<T> &values, std::size_t extra)
{
    const auto needed = values.size() + extra;
    if (needed > values.capacity())
        values.reserve(std::max(needed, values.capacity() + values.capacity() / 2));
}

template <class T> T Read(std::istream &in)
{
    T value{};
    if (!in.read(reinterpret_cast<char *>(&value), sizeof(value)))
        throw std::runtime_error("truncated resource pack");
    return value;
}

std::string ReadString(std::istream &in)
{
    auto size = Read<std::uint32_t>(in);
    if (size > 256)
        throw std::runtime_error("invalid resource name");
    std::string result(size, '\0');
    if (!in.read(result.data(), size))
        throw std::runtime_error("truncated resource name");
    return result;
}

template <class T> void ReadVector(std::istream &in, std::vector<T> &values, std::size_t count)
{
    values.resize(count);
    if (!in.read(reinterpret_cast<char *>(values.data()), std::streamsize(count * sizeof(T))))
        throw std::runtime_error("truncated resource buffer");
}

bool Visible(float x, float z, const RpgGame &game, float radius)
{
    return std::abs(x - game.player.x) <= radius && std::abs(z - game.player.z) <= radius;
}
}

bool WorldResources::Load(const std::filesystem::path &path)
{
    Shutdown();
    static_assert(sizeof(SourceVertex) == 72, "Resource vertex format changed");
    static_assert(sizeof(Vertex) == 48, "GPU vertex format changed");
    try
    {
        std::ifstream in(path, std::ios::binary);
        if (!in || Read<std::uint32_t>(in) != 0x314b5052 || Read<std::uint32_t>(in) != 1)
            throw std::runtime_error("missing or unsupported kingdom.rpack");
        auto count = Read<std::uint32_t>(in);
        auto width = Read<std::uint32_t>(in), height = Read<std::uint32_t>(in);
        if (!count || count > 256 || width != 2080 || height != 2080)
            throw std::runtime_error("invalid resource pack header");
        std::vector<unsigned char> pixels;
        ReadVector(in, pixels, std::size_t(width) * height * 4);
        glGenTextures(1, &atlas);
        glBindTexture(GL_TEXTURE_2D, atlas);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        // A padded atlas uses linear sampling; mipmaps would mix unrelated material cells.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        for (unsigned i = 0; i < count; ++i)
        {
            auto name = ReadString(in);
            auto vertices = Read<std::uint32_t>(in), indices = Read<std::uint32_t>(in);
            auto bones = Read<std::uint32_t>(in), clips = Read<std::uint32_t>(in);
            if (!vertices || vertices > 1000000 || indices > 3000000 || indices % 3 || bones > 64 ||
                clips > 64 || assets.count(name))
                throw std::runtime_error("invalid resource mesh limits");
            Asset asset;
            asset.bones = bones;
            for (auto &bound : asset.bounds)
            {
                bound = Read<float>(in);
                if (!std::isfinite(bound))
                    throw std::runtime_error("non-finite resource bounds");
            }
            ReadVector(in, asset.vertices, vertices);
            ReadVector(in, asset.indices, indices);
            for (auto index : asset.indices)
                if (index >= vertices)
                    throw std::runtime_error("invalid resource index");
            for (const auto &v : asset.vertices)
            {
                for (float n : v.position)
                    if (!std::isfinite(n))
                        throw std::runtime_error("non-finite resource position");
                for (int j = 0; j < 4; ++j)
                    if (!std::isfinite(v.weights[j]) || (v.weights[j] > 0 && v.joints[j] >= bones))
                        throw std::runtime_error("invalid skin influence");
            }
            for (unsigned c = 0; c < clips; ++c)
            {
                auto clipName = ReadString(in);
                Clip clip;
                clip.duration = Read<float>(in);
                clip.frames = Read<std::uint32_t>(in);
                if (!bones || !std::isfinite(clip.duration) || clip.duration < 0 ||
                    clip.frames < 2 || clip.frames > 4096)
                    throw std::runtime_error("invalid animation limits");
                ReadVector(in, clip.matrices, std::size_t(clip.frames) * bones * 16);
                for (float n : clip.matrices)
                    if (!std::isfinite(n))
                        throw std::runtime_error("non-finite animation matrix");
                asset.clips.emplace(clipName, std::move(clip));
            }
            assets.emplace(name, std::move(asset));
        }
        for (const char *required : {"NPC_Resident",
                                     "NPC_Worker",
                                     "NPC_Scout",
                                     "Wolf",
                                     "Tiger",
                                     "Fox",
                                     "Deer",
                                     "Home_Prefab",
                                     "Terrain_Grass_2x2",
                                     "Terrain_Soil_2x2",
                                     "Tree_Pine_01",
                                     "Rock_01"})
            if (!assets.count(required))
                throw std::runtime_error("required resource asset missing");
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        std::fprintf(stderr, "Resource pack: %s\n", error.c_str());
        Shutdown();
        return false;
    }
}

void WorldResources::Free(Batch &batch)
{
    if (batch.vbo)
        glDeleteBuffers(1, &batch.vbo);
    if (batch.ibo)
        glDeleteBuffers(1, &batch.ibo);
    batch = {};
}

void WorldResources::ClearScene()
{
    for (auto &entry : chunks)
    {
        Free(entry.second.ground);
        Free(entry.second.objects);
    }
    for (auto &entry : towns)
        Free(entry.second);
    chunks.clear();
    towns.clear();
    poses.clear();
    skinPoses.clear();
    activeSkinPoses = 0;
    Free(actors);
    actorTime = -1;
}

void WorldResources::Shutdown()
{
    ClearScene();
    if (atlas)
        glDeleteTextures(1, &atlas);
    atlas = 0;
    assets.clear();
    seeded = false;
}

void WorldResources::BeginFrame()
{
    ++frame;
    drawCalls = triangles = cacheBuilds = 0;
    preparationMs = poseMs = 0;
    skinEvaluations = skinReuses = skinnedVertices = 0;
    skinEvaluationMs = actorAssemblyMs = actorUploadMs = 0;
}

std::vector<float> WorldResources::Sample(const Asset &asset,
                                          const std::string &name,
                                          float time) const
{
    auto found = asset.clips.find(name);
    if (found == asset.clips.end())
        return {};
    const auto &clip = found->second;
    float clock = std::max(0.0f, time);
    if (name == "Attack" || name == "Jump" || name == "Walk_Stop")
        clock = std::min(clock, clip.duration);
    else if (clip.duration > 0)
        clock = std::fmod(clock, clip.duration);
    float sample = clip.duration > 0 ? clock / clip.duration * (clip.frames - 1) : 0;
    auto a = std::min(unsigned(sample), clip.frames - 1);
    auto b = std::min(a + 1, clip.frames - 1);
    float t = sample - a;
    std::size_t stride = asset.bones * 16;
    std::vector<float> pose(stride);
    for (std::size_t i = 0; i < stride; ++i)
        pose[i] = clip.matrices[a * stride + i] * (1 - t) + clip.matrices[b * stride + i] * t;
    return pose;
}

const std::vector<WorldResources::SkinVertex> &WorldResources::Skin(const Asset &asset,
                                                                    const std::string &clip,
                                                                    float clock,
                                                                    const std::string &previous,
                                                                    float previousTime,
                                                                    float blend)
{
    // Previous pose inputs have no effect once the transition has finished.
    const std::string oldClip = blend < 1 ? previous : "";
    const float oldTime = oldClip.empty() ? 0 : previousTime;
    const float mix = oldClip.empty() ? 1 : blend;
    for (std::size_t i = 0; i < activeSkinPoses; ++i)
    {
        const auto &entry = skinPoses[i];
        if (entry.asset == &asset && entry.clip == clip && entry.clock == clock &&
            entry.previous == oldClip && entry.previousTime == oldTime && entry.blend == mix)
        {
            ++skinReuses;
            return entry.vertices;
        }
    }
    auto started = std::chrono::steady_clock::now();
    if (activeSkinPoses == skinPoses.size())
        skinPoses.emplace_back();
    auto &entry = skinPoses[activeSkinPoses++];
    entry.asset = &asset;
    entry.clip = clip;
    entry.clock = clock;
    entry.previous = oldClip;
    entry.previousTime = oldTime;
    entry.blend = mix;
    auto pose = Sample(asset, clip, clock);
    if (!pose.empty() && mix < 1 && !oldClip.empty())
    {
        auto old = Sample(asset, oldClip, oldTime);
        if (old.size() == pose.size())
            for (std::size_t i = 0; i < pose.size(); ++i)
                pose[i] = old[i] * (1 - mix) + pose[i] * mix;
    }
    entry.vertices.resize(asset.vertices.size());
    for (std::size_t index = 0; index < asset.vertices.size(); ++index)
    {
        const auto &source = asset.vertices[index];
        auto &vertex = entry.vertices[index];
        if (pose.empty())
        {
            std::copy_n(source.position, 3, vertex.position);
            std::copy_n(source.normal, 3, vertex.normal);
            continue;
        }
        std::fill_n(vertex.position, 3, 0.0f);
        std::fill_n(vertex.normal, 3, 0.0f);
        for (int influence = 0; influence < 4; ++influence)
        {
            float weight = source.weights[influence];
            if (weight <= 0)
                continue;
            const auto *m = &pose[source.joints[influence] * 16];
            for (int axis = 0; axis < 3; ++axis)
            {
                vertex.position[axis] +=
                    weight * (m[axis] * source.position[0] + m[4 + axis] * source.position[1] +
                              m[8 + axis] * source.position[2] + m[12 + axis]);
                vertex.normal[axis] +=
                    weight * (m[axis] * source.normal[0] + m[4 + axis] * source.normal[1] +
                              m[8 + axis] * source.normal[2]);
            }
        }
    }
    ++skinEvaluations;
    skinnedVertices += unsigned(asset.vertices.size());
    skinEvaluationMs += Milliseconds(started);
    return entry.vertices;
}

void WorldResources::Append(Batch &batch,
                            const std::string &name,
                            float x,
                            float y,
                            float z,
                            float sx,
                            float sy,
                            float sz,
                            float yaw,
                            const std::string &clip,
                            float clock,
                            const std::string &previous,
                            float previousTime,
                            float blend,
                            bool flatten)
{
    auto found = assets.find(name);
    if (found == assets.end())
        return;
    const auto &asset = found->second;
    const std::vector<SkinVertex> *skin = nullptr;
    if (asset.bones && !clip.empty() && asset.clips.find(clip) != asset.clips.end())
        skin = &Skin(asset, clip, clock, previous, previousTime, blend);
    std::uint32_t base = std::uint32_t(batch.vertices.size());
    ReserveAdditional(batch.vertices, asset.vertices.size());
    float c = std::cos(yaw), s = std::sin(yaw);
    for (std::size_t index = 0; index < asset.vertices.size(); ++index)
    {
        const auto &source = asset.vertices[index];
        Vertex vertex{};
        const float *p = skin ? (*skin)[index].position : source.position;
        float n[3];
        std::copy_n(skin ? (*skin)[index].normal : source.normal, 3, n);
        vertex.position[0] = x + p[0] * sx * c + p[2] * sz * s;
        vertex.position[1] = flatten ? y : y + p[1] * sy;
        vertex.position[2] = z - p[0] * sx * s + p[2] * sz * c;
        n[0] /= sx;
        n[1] /= sy;
        n[2] /= sz;
        vertex.normal[0] = n[0] * c + n[2] * s;
        vertex.normal[1] = n[1];
        vertex.normal[2] = -n[0] * s + n[2] * c;
        if (flatten)
        {
            vertex.normal[0] = vertex.normal[2] = 0;
            vertex.normal[1] = 1;
        }
        std::copy_n(source.color, 3, vertex.color);
        std::copy_n(source.uv, 3, vertex.uv);
        batch.vertices.push_back(vertex);
    }
    ReserveAdditional(batch.indices, asset.indices.size());
    for (auto index : asset.indices)
        batch.indices.push_back(base + index);
}

void WorldResources::Fit(
    Batch &batch, const std::string &name, float x, float z, float width, float depth, float height)
{
    auto found = assets.find(name);
    if (found == assets.end())
        return;
    const auto &b = found->second.bounds;
    float sx = width / std::max(.01f, b[3] - b[0]), sz = depth / std::max(.01f, b[5] - b[2]);
    float sy = height > 0 ? height / std::max(.01f, b[4] - b[1]) : std::min(sx, sz);
    Append(batch,
           name,
           x - (b[0] + b[3]) * .5f * sx,
           -b[1] * sy,
           z - (b[2] + b[5]) * .5f * sz,
           sx,
           sy,
           sz);
}

void WorldResources::Upload(Batch &batch, bool dynamic)
{
    if (batch.vertices.empty())
    {
        batch.indexCount = batch.vertexCount = 0;
        return;
    }
    if (!batch.vbo)
        glGenBuffers(1, &batch.vbo);
    if (!batch.ibo)
        glGenBuffers(1, &batch.ibo);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 batch.vertices.size() * sizeof(Vertex),
                 batch.vertices.data(),
                 dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 batch.indices.size() * sizeof(std::uint32_t),
                 batch.indices.data(),
                 dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    batch.vertexCount = batch.vertices.size();
    batch.indexCount = batch.indices.size();
    batch.vertices.clear();
    batch.indices.clear();
    if (!dynamic)
    {
        batch.vertices.shrink_to_fit();
        batch.indices.shrink_to_fit();
    }
}

void WorldResources::Draw(const Batch &batch)
{
    if (!batch.indexCount)
        return;
    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.ibo);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(
        3, GL_FLOAT, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
    glNormalPointer(GL_FLOAT, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, normal)));
    glColorPointer(3, GL_FLOAT, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
    glTexCoordPointer(3, GL_FLOAT, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, uv)));
    glDrawElements(GL_TRIANGLES, GLsizei(batch.indexCount), GL_UNSIGNED_INT, nullptr);
    ++drawCalls;
    triangles += unsigned(batch.indexCount / 3);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void WorldResources::BuildChunk(Chunk &chunk, const RpgGame &game, int cx, int cz)
{
    for (int z = cz * ChunkSize; z < (cz + 1) * ChunkSize; z += 2)
        for (int x = cx * ChunkSize; x < (cx + 1) * ChunkSize; x += 2)
        {
            bool soil = novel::OnRoad(float(x + 1), float(z + 1)) ||
                        TownNpc::InTown(float(x + 1), float(z + 1));
            Append(chunk.ground,
                   soil ? "Terrain_Soil_2x2" : "Terrain_Grass_2x2",
                   x + 1.0f,
                   0,
                   z + 1.0f,
                   1,
                   1,
                   1,
                   0,
                   "",
                   0,
                   "",
                   0,
                   1,
                   true);
        }
    for (int z = cz * ChunkSize; z < (cz + 1) * ChunkSize; ++z)
        for (int x = cx * ChunkSize; x < (cx + 1) * ChunkSize; ++x)
        {
            auto ground = game.Terrain(x, z);
            unsigned hash = unsigned(x) * 0x9e3779b9u ^ unsigned(z) * 0x85ebca6bu ^ game.seed;
            if (ground == RpgGame::Ground::Tree)
                Fit(chunk.objects,
                    hash % 3 ? "Tree_Pine_01" : "Tree_Broadleaf_01",
                    float(x),
                    float(z),
                    1.35f,
                    1.35f,
                    2.5f + (hash % 5) * .12f);
            else if (ground == RpgGame::Ground::Rock)
                Fit(chunk.objects,
                    hash % 2 ? "Rock_01" : "Rock_02",
                    float(x),
                    float(z),
                    .8f,
                    .8f,
                    .6f);
            else if (!TownNpc::InTown(float(x), float(z)) && !novel::OnRoad(float(x), float(z)) &&
                     hash % 47 == 0)
                Append(
                    chunk.objects, "Grass_Patch_01", float(x), .005f, float(z), .65f, .65f, .65f);
        }
    Upload(chunk.ground);
    Upload(chunk.objects);
    ++cacheBuilds;
}

void WorldResources::BuildTown(Batch &batch, int village)
{
    const auto &v = novel::Villages()[village];
    for (std::size_t i = village * 6; i < (village + 1) * 6; ++i)
    {
        const auto &b = TownNpc::Buildings()[i];
        Fit(batch, "Home_Prefab", b.x, b.z, b.width, b.depth, 2.8f);
    }
    // Furnish the work yards; leave the central road and plaza open.
    Append(batch, "Table_Work", v.x - 6, 0, v.z + 3, .7f, .7f, .7f);
    Append(batch, "Chair", v.x - 7.2f, 0, v.z + 3, .7f, .7f, .7f, Pi * .5f);
    Append(batch, "Supply_Crate", v.x + 8, 0, v.z + 5, .7f, .7f, .7f);
    Append(batch, "Sack", v.x + 7, 0, v.z + 5, .7f, .7f, .7f);
    Append(batch, "Bucket", v.x + 1, 0, v.z - 3, .7f, .7f, .7f);
    Append(batch, "Lamp_Utility", v.x - 3, 0, v.z + 3, 1, 1, 1);
    for (int i = -1; i <= 1; ++i)
        Append(batch, "Fence_Timber_2m", v.x + i * 2, 0, v.z + 10, 1, .7f, 1);
    // The existing shader flame remains above a resource cooking brazier.
    Append(batch, "Cooking_Pot", v.x + 2, 0, v.z + 3, .65f, .65f, .65f);
    Upload(batch);
    ++cacheBuilds;
}

void WorldResources::Environment(const RpgGame &game, int radius, bool shadow)
{
    if (!Ready())
        return;
    auto started = std::chrono::steady_clock::now();
    if (!seeded || seed != game.seed || game.time < actorTime)
    {
        ClearScene();
        seed = game.seed;
        seeded = true;
    }
    int minX = int(std::floor((game.player.x - radius) / ChunkSize));
    int maxX = int(std::floor((game.player.x + radius) / ChunkSize));
    int minZ = int(std::floor((game.player.z - radius) / ChunkSize));
    int maxZ = int(std::floor((game.player.z + radius) / ChunkSize));
    // Evict before allocating the next view. Memory is bounded by the current view plus one ring.
    for (auto it = chunks.begin(); it != chunks.end();)
        if (it->first.first < minX - 1 || it->first.first > maxX + 1 ||
            it->first.second < minZ - 1 || it->first.second > maxZ + 1)
        {
            Free(it->second.ground);
            Free(it->second.objects);
            it = chunks.erase(it);
        }
        else
            ++it;
    for (int z = minZ; z <= maxZ; ++z)
        for (int x = minX; x <= maxX; ++x)
        {
            auto key = std::make_pair(x, z);
            auto found = chunks.find(key);
            if (found == chunks.end())
            {
                auto inserted = chunks.emplace(key, Chunk{});
                found = inserted.first;
                BuildChunk(found->second, game, x, z);
            }
            found->second.used = frame;
            if (!shadow)
                Draw(found->second.ground);
            Draw(found->second.objects);
        }
    for (int i = 0; i < novel::VillageCount; ++i)
        if (Visible(novel::Villages()[i].x, novel::Villages()[i].z, game, float(radius + 12)))
        {
            auto found = towns.find(i);
            if (found == towns.end())
            {
                found = towns.emplace(i, Batch{}).first;
                BuildTown(found->second, i);
            }
            Draw(found->second);
        }
    if (!shadow)
    {
        // Fill the already-retained one-chunk ring before movement makes it visible.
        // At most one extra chunk per frame; never omit a required visible chunk.
        std::pair<int, int> next{};
        bool pending = false;
        float bestDistance = 0;
        for (int z = minZ - 1; z <= maxZ + 1; ++z)
            for (int x = minX - 1; x <= maxX + 1; ++x)
            {
                auto key = std::make_pair(x, z);
                if (chunks.find(key) != chunks.end())
                    continue;
                float dx = (x + .5f) * ChunkSize - game.player.x;
                float dz = (z + .5f) * ChunkSize - game.player.z;
                float distance = dx * dx + dz * dz;
                if (!pending || distance < bestDistance)
                {
                    next = key;
                    bestDistance = distance;
                    pending = true;
                }
            }
        if (pending)
        {
            auto inserted = chunks.emplace(next, Chunk{});
            BuildChunk(inserted.first->second, game, next.first, next.second);
        }
    }
    preparationMs += Milliseconds(started);
}

void WorldResources::AddActor(const RpgGame &game,
                              int id,
                              const std::string &asset,
                              float x,
                              float z,
                              bool moving,
                              float attackStarted,
                              float speed,
                              float targetX,
                              float targetZ)
{
    auto &state = poses[id];
    float dx = x - state.x, dz = z - state.z;
    bool attacking = game.time - attackStarted >= 0 && game.time - attackStarted < .6f;
    if (!state.initialized)
    {
        state.x = x;
        state.z = z;
        state.initialized = true;
        dx = dz = 0;
    }
    float desired = state.yaw;
    if (attacking)
        desired = std::atan2(targetX - x, targetZ - z);
    else if (moving && std::hypot(dx, dz) > .00001f)
        desired = std::atan2(dx, dz);
    float angle = std::remainder(desired - state.yaw, 2 * Pi);
    state.yaw += std::clamp(angle, -.22f, .22f);
    state.x = x;
    state.z = z;
    const std::string clip = attacking ? "Attack" : moving ? (id == -1 ? "Run" : "Walk") : "Idle";
    float clock = attacking ? game.time - attackStarted : game.time * speed;
    if (state.clip != clip)
    {
        state.previous = state.clip;
        state.previousTime = state.clock;
        state.clip = clip;
        state.changedAt = game.time;
    }
    state.clock = clock;
    Append(actors,
           asset,
           x,
           .005f,
           z,
           1,
           1,
           1,
           state.yaw,
           clip,
           clock,
           state.previous,
           state.previousTime,
           std::clamp((game.time - state.changedAt) / .16f, 0.0f, 1.0f));
}

void WorldResources::BuildActors(const RpgGame &game, int radius)
{
    auto started = std::chrono::steady_clock::now();
    actors.vertices.clear();
    actors.indices.clear();
    activeSkinPoses = 0;
    if (game.hp <= 0)
        Append(actors,
               "NPC_Worker",
               game.player.x,
               .005f,
               game.player.z,
               1,
               1,
               1,
               0,
               "Pose_Crouch",
               0);
    if (game.hp > 0)
        AddActor(game,
                 -1,
                 "NPC_Worker",
                 game.player.x,
                 game.player.z,
                 game.moving,
                 game.attackStarted,
                 game.playerMoveSpeed / 2.303f,
                 game.aim.x,
                 game.aim.z);
    for (const auto &n : game.npcs)
        if (n.hp > 0 && Visible(n.position.x, n.position.z, game, float(radius + 2)))
        {
            const char *asset = n.role == NpcRole::Guard || n.role == NpcRole::Hunter ? "NPC_Scout"
                                : n.role == NpcRole::Merchant || n.role == NpcRole::Medic
                                    ? "NPC_Resident"
                                    : "NPC_Worker";
            AddActor(game,
                     n.id,
                     asset,
                     n.position.x,
                     n.position.z,
                     n.moving,
                     n.attackStarted,
                     n.walkSpeed / .968f,
                     n.goal.x,
                     n.goal.z);
        }
    const char *species[] = {"Tiger", "Wolf", "Fox", "Deer"};
    const float stride[] = {.738f, .601f, .389f, .581f};
    for (int i = 0; i < int(game.enemies.size()); ++i)
    {
        const auto &e = game.enemies[i];
        if (e.hp <= 0 || !Visible(e.position.x, e.position.z, game, float(radius + 2)))
            continue;
        int kind = std::clamp(e.appearance, 0, 3);
        AddActor(game,
                 1000 + i,
                 species[kind],
                 e.position.x,
                 e.position.z,
                 e.moving,
                 e.attackStarted,
                 (e.action == BanditAction::Patrol ? .99f : 1.65f) / stride[kind],
                 game.player.x,
                 game.player.z);
    }
    for (const auto &drop : game.drops)
        if (Visible(drop.position.x, drop.position.z, game, float(radius)))
            Append(actors,
                   drop.potion ? "Bucket" : "Sack",
                   drop.position.x,
                   .02f,
                   drop.position.z,
                   .3f,
                   .3f,
                   .3f);
    auto uploadStarted = std::chrono::steady_clock::now();
    actorAssemblyMs = Milliseconds(started) - skinEvaluationMs;
    Upload(actors, true);
    actorUploadMs = Milliseconds(uploadStarted);
    // Reuse the evaluated geometry in both shadow and color passes.
    poseMs += Milliseconds(started);
    actorTime = game.time;
    actorRadius = radius;
}

void WorldResources::Actors(const RpgGame &game, int radius)
{
    if (!Ready())
        return;
    if (actorTime != game.time || actorRadius != radius)
        BuildActors(game, radius);
    Draw(actors);
}

void WorldResources::Profile(float gameTime)
{
    ++windowFrames;
    windowCacheBuilds += cacheBuilds;
    peakDrawCalls = std::max(peakDrawCalls, drawCalls);
    peakPreparationMs = std::max(peakPreparationMs, preparationMs);
    peakPoseMs = std::max(peakPoseMs, poseMs);
    if (gameTime - lastProfileTime < 1 && gameTime >= lastProfileTime)
        return;
    lastProfileTime = gameTime;
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    auto directory = std::filesystem::path(executable).parent_path() / "Profiles";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    auto path = directory / "resource_render.jsonl";
    if (std::filesystem::exists(path, ec) && std::filesystem::file_size(path, ec) > 5 * 1024 * 1024)
    {
        auto previous = directory / "resource_render.previous.jsonl";
        std::filesystem::remove(previous, ec);
        ec.clear();
        std::filesystem::rename(path, previous, ec);
        if (ec)
            return;
    }
    std::ofstream out(path, std::ios::app);
    out.imbue(std::locale::classic());
    out << "{\"event\":\"resource_render_frame\",\"frame\":" << frame
        << ",\"game_seconds\":" << gameTime << ",\"mesh_draw_calls\":" << drawCalls
        << ",\"submitted_triangles\":" << triangles << ",\"static_chunks\":" << chunks.size()
        << ",\"static_cache_builds\":" << cacheBuilds << ",\"cpu_environment_ms\":" << preparationMs
        << ",\"cpu_skinning_and_upload_ms\":" << poseMs
        << ",\"skin_evaluations\":" << skinEvaluations << ",\"skin_cache_hits\":" << skinReuses
        << ",\"skinned_vertices\":" << skinnedVertices
        << ",\"cpu_pose_evaluation_ms\":" << skinEvaluationMs
        << ",\"cpu_actor_assembly_ms\":" << actorAssemblyMs
        << ",\"cpu_actor_upload_ms\":" << actorUploadMs << ",\"window_frames\":" << windowFrames
        << ",\"static_cache_builds_window\":" << windowCacheBuilds
        << ",\"mesh_draw_calls_peak_window\":" << peakDrawCalls
        << ",\"cpu_environment_peak_ms\":" << peakPreparationMs
        << ",\"cpu_skinning_and_upload_peak_ms\":" << peakPoseMs << "}\n";
    windowFrames = windowCacheBuilds = peakDrawCalls = 0;
    peakPreparationMs = peakPoseMs = 0;
}
