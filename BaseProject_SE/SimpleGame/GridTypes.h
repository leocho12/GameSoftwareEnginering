#pragma once

struct Cell
{
    int x = -1, z = -1;

    bool operator==(const Cell &p) const
    {
        return x == p.x && z == p.z;
    }

    bool operator!=(const Cell &p) const
    {
        return !(*this == p);
    }
};
