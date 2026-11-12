#pragma once
#include "OpenGL.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

class RpgGame;

// Runtime representation of the delivered Resource GLBs. One indexed draw per batch.
class WorldResources
{
  public:
    bool Load(const std::filesystem::path &path);
    void Shutdown();
    void BeginFrame();
    void Environment(const RpgGame &game, int radius, bool shadow);
    void Actors(const RpgGame &game, int radius);
    void Profile(float gameTime);

    bool Ready() const
    {
        return atlas != 0 && !assets.empty();
    }

    std::string error;

  private:
    friend struct WorldResourcesSkinTests;

    struct SourceVertex
    {
        float position[3], normal[3], color[3], uv[3];
        std::uint16_t joints[4];
        float weights[4];
    };

    struct Vertex
    {
        float position[3], normal[3], color[3], uv[3];
    };

    struct Clip
    {
        float duration = 0;
        std::uint32_t frames = 0;
        std::vector<float> matrices;
    };

    struct Asset
    {
        std::vector<SourceVertex> vertices;
        std::vector<std::uint32_t> indices;
        std::uint32_t bones = 0;
        std::array<float, 6> bounds{};
        std::map<std::string, Clip> clips;
    };

    struct Batch
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        GLuint vbo = 0, ibo = 0;
        std::size_t vertexCount = 0, indexCount = 0;
    };

    struct Chunk
    {
        Batch ground, objects;
        std::uint64_t used = 0;
    };

    struct Pose
    {
        std::string clip = "Idle", previous = "Idle";
        float changedAt = -100, previousTime = 0, clock = 0, yaw = 0;
        float x = 0, z = 0;
        bool initialized = false;
    };

    struct SkinVertex
    {
        float position[3], normal[3];
    };

    struct SkinPose
    {
        const Asset *asset = nullptr;
        std::string clip, previous;
        float clock = 0, previousTime = 0, blend = 1;
        std::vector<SkinVertex> vertices;
    };

    std::map<std::string, Asset> assets;
    std::map<std::pair<int, int>, Chunk> chunks;
    std::map<int, Batch> towns;
    std::map<int, Pose> poses;
    // Local-space deformation is shared only for identical inputs within one actor build.
    // Reuse storage across frames; asset reload/scene reset discards all entries.
    std::vector<SkinPose> skinPoses;
    std::size_t activeSkinPoses = 0;
    Batch actors;
    GLuint atlas = 0;
    std::uint32_t seed = 0;
    bool seeded = false;
    float actorTime = -1, lastProfileTime = -100;
    int actorRadius = 0;
    std::uint64_t frame = 0;
    unsigned drawCalls = 0, triangles = 0, cacheBuilds = 0;
    float preparationMs = 0, poseMs = 0;
    unsigned skinEvaluations = 0, skinReuses = 0, skinnedVertices = 0;
    float skinEvaluationMs = 0, actorAssemblyMs = 0, actorUploadMs = 0;
    unsigned windowFrames = 0, windowCacheBuilds = 0, peakDrawCalls = 0;
    float peakPreparationMs = 0, peakPoseMs = 0;
    void Append(Batch &batch,
                const std::string &name,
                float x,
                float y,
                float z,
                float sx = 1,
                float sy = 1,
                float sz = 1,
                float yaw = 0,
                const std::string &clip = "",
                float clock = 0,
                const std::string &previous = "",
                float previousTime = 0,
                float blend = 1,
                bool flatten = false);
    void Fit(Batch &batch,
             const std::string &name,
             float x,
             float z,
             float width,
             float depth,
             float height = 0);
    void Upload(Batch &batch, bool dynamic = false);
    void Draw(const Batch &batch);
    static void Free(Batch &batch);
    void ClearScene();
    void BuildChunk(Chunk &chunk, const RpgGame &game, int x, int z);
    void BuildTown(Batch &batch, int village);
    void BuildActors(const RpgGame &game, int radius);
    void AddActor(const RpgGame &game,
                  int id,
                  const std::string &asset,
                  float x,
                  float z,
                  bool moving,
                  float attackStarted,
                  float speed,
                  float targetX,
                  float targetZ);
    std::vector<float> Sample(const Asset &asset, const std::string &clip, float time) const;
    const std::vector<SkinVertex> &Skin(const Asset &asset,
                                        const std::string &clip,
                                        float clock,
                                        const std::string &previous,
                                        float previousTime,
                                        float blend);
};
