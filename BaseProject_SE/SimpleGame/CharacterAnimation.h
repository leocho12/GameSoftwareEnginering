#pragma once
#include "Dependencies/glew.h"
#include <map>
#include <string>
#include <vector>

// Offline-baked, ordered meshes. No Unity/Spine runtime is required by the game.
class CharacterAnimation
{
  public:
    bool Load(const std::wstring &path);

    bool Ready() const
    {
        return !clips.empty();
    }

    bool Draw(GLuint texture,
              const std::string &clip,
              float time,
              bool loop,
              float x,
              float y,
              float z,
              bool mirror,
              bool shadow) const;

    void Clear()
    {
        clips.clear();
    }

  private:
    struct Vertex
    {
        float x, y, u, v, r, g, b, a;
    };

    struct Clip
    {
        float duration = 0;
        std::vector<std::vector<Vertex>> frames;
    };

    std::map<std::string, Clip> clips;
};
