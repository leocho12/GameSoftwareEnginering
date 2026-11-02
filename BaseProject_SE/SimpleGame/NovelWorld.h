#pragma once
#include <array>
#include <cmath>

// These five compact settlements are a gameplay adaptation, not the novel's map.
namespace novel
{
constexpr int VillageCount = 5;
constexpr int ResidentsPerVillage = 20;
constexpr int CorridorCount = VillageCount - 1;
constexpr int MonstersPerCorridor = 50;
constexpr float PlayerSpeed = 3.6f;
constexpr float VillageSpacing = PlayerSpeed * 20.0f;

struct Village
{
    const char *id;
    const char *name;
    const char *description;
    float x, z;
};

inline const std::array<Village, VillageCount> &Villages()
{
    static const std::array<Village, VillageCount> villages{
        {{"aplia", "아플리아 외곽", "아플리아 아카데미로 향하는 교역 거점", 3, 3},
         {"kadinak", "카디낙 길목", "밤의 도시 카디낙 인근의 여관과 장터", 75, 3},
         {"hameln", "하멜른", "쿠락트 산맥 인근의 은퇴자 마을", 147, 3},
         {"kirmelt", "키르멜트 후방", "협곡 전선을 지원하는 후방 정착지", 219, 3},
         {"kurakt", "쿠락트 산록", "북부 산악 길의 작은 교역 거점", 291, 3}}};
    return villages;
}

inline int VillageAt(float x, float z)
{
    for (int i = 0; i < VillageCount; ++i)
    {
        const auto &v = Villages()[i];
        if (x >= v.x - 11 && x <= v.x + 11 && z >= v.z - 11 && z <= v.z + 11)
            return i;
    }
    return -1;
}

inline bool OnRoad(float x, float z)
{
    return x >= -8 && x <= Villages().back().x + 11 && z >= 1 && z <= 5;
}

inline bool PropBlocked(float x, float z)
{
    for (const auto &v : Villages())
    {
        if (std::abs(x - (v.x - 6)) <= .8f && std::abs(z - (v.z + 3)) <= .5f)
            return true;
        if (std::abs(x - (v.x + 8)) <= .5f && std::abs(z - (v.z + 5)) <= .5f)
            return true;
        if (std::abs(x - v.x) <= 3 && std::abs(z - (v.z + 10)) <= .25f)
            return true;
    }
    return false;
}

}
