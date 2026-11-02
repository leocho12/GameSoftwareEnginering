// Link WorldResources and the normal RPG/OpenGL libraries. No window or GL context is used:
// the test calls only CPU deformation and clears empty GPU batches.
#include "../BaseProject_SE/SimpleGame/WorldResources.h"
#include <cassert>
#include <cmath>

#ifdef NDEBUG
#error Build this regression without NDEBUG so the assertions execute.
#endif

#ifdef WORLD_RESOURCES_CPU_TEST_STUBS
// Standalone linking only: these world/render dependencies must never run in a skin test.
// Omit this flag when linking the full RPG sources instead.
#include "../BaseProject_SE/SimpleGame/RpgGame.h"
#include <cstdlib>

bool TownNpc::InTown(float, float)
{
    std::abort();
}

const std::vector<NpcBuilding> &TownNpc::Buildings()
{
    std::abort();
}

RpgGame::Ground RpgGame::Terrain(int, int) const
{
    std::abort();
}
#endif

struct WorldResourcesSkinTests
{
    static WorldResources::Clip Clip(float startX, float endX)
    {
        WorldResources::Clip clip;
        clip.duration = 1;
        clip.frames = 2;
        clip.matrices.assign(32, 0);
        for (int frame = 0; frame < 2; ++frame)
        {
            for (int axis = 0; axis < 4; ++axis)
                clip.matrices[frame * 16 + axis * 5] = 1;
            clip.matrices[frame * 16 + 12] = frame == 0 ? startX : endX;
        }
        return clip;
    }

    static void Run()
    {
        WorldResources resources;
        WorldResources::Asset asset;
        asset.bones = 1;
        WorldResources::SourceVertex vertex{};
        vertex.position[0] = 1;
        vertex.position[1] = 2;
        vertex.position[2] = 3;
        vertex.normal[1] = 1;
        vertex.weights[0] = 1;
        asset.vertices.push_back(vertex);
        asset.clips["Idle"] = Clip(0, 4);
        asset.clips["Walk"] = Clip(8, 8);

        auto first = resources.Skin(asset, "Idle", .5f, "", 0, 1)[0];
        assert(first.position[0] == 3 && first.position[1] == 2 && first.position[2] == 3);
        assert(first.normal[0] == 0 && first.normal[1] == 1 && first.normal[2] == 0);
        resources.Skin(asset, "Idle", .5f, "Walk", 99, 1);
        assert(resources.skinEvaluations == 1 && resources.skinReuses == 1);
        // Completed transitions ignore previous-pose fields, active transitions do not.
        auto blended = resources.Skin(asset, "Idle", .5f, "Walk", 0, .25f)[0];
        assert(std::abs(blended.position[0] - 7.5f) < .00001f);
        resources.Skin(asset, "Idle", .75f, "", 0, 1);
        assert(resources.skinEvaluations == 3);

        auto other = asset;
        other.vertices[0].position[0] = 5;
        auto differentAsset = resources.Skin(other, "Idle", .5f, "", 0, 1)[0];
        assert(differentAsset.position[0] == 7 && resources.skinEvaluations == 4);
        auto differentClip = resources.Skin(asset, "Walk", .5f, "", 0, 1)[0];
        assert(differentClip.position[0] == 9 && resources.skinEvaluations == 5);

        resources.activeSkinPoses = 0; // The production actor-build boundary.
        auto nextFrame = resources.Skin(asset, "Idle", .5f, "", 0, 1)[0];
        assert(nextFrame.position[0] == 3 && resources.skinEvaluations == 6);

        auto weighted = asset;
        weighted.bones = 2;
        weighted.vertices[0].weights[0] = .25f;
        weighted.vertices[0].weights[1] = .75f;
        weighted.vertices[0].joints[1] = 1;
        auto &twoBones = weighted.clips["Idle"];
        twoBones.matrices.assign(64, 0);
        for (int frame = 0; frame < 2; ++frame)
            for (int bone = 0; bone < 2; ++bone)
            {
                int offset = frame * 32 + bone * 16;
                for (int axis = 0; axis < 4; ++axis)
                    twoBones.matrices[offset + axis * 5] = 1;
                if (bone == 1)
                {
                    twoBones.matrices[offset + 12] = 8;
                    twoBones.matrices[offset + 5] = 2;
                }
            }
        auto mixed = resources.Skin(weighted, "Idle", .5f, "", 0, 1)[0];
        assert(mixed.position[0] == 7 && mixed.position[1] == 3.5f && mixed.position[2] == 3);
        assert(mixed.normal[1] == 1.75f);
        resources.ClearScene(); // No stale asset pointers survive scene/seed changes.
        assert(resources.skinPoses.empty() && resources.activeSkinPoses == 0);
    }
};

int main()
{
    WorldResourcesSkinTests::Run();
}
