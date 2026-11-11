#include "stdafx.h"
#include "RenderGeometry.h"
#include <GL/glew.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float Pi = 3.14159265358979323846f;

struct Vertex3
{
    float x;
    float y;
    float z;
};

void Disk(float radius, int slices, float z, float normalZ, bool reverse)
{
    glNormal3f(0, 0, normalZ);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, 0, z);
    for (int i = 0; i <= slices; ++i)
    {
        const int index = reverse ? slices - i : i;
        const float angle = 2 * Pi * float(index) / float(slices);
        glVertex3f(radius * std::cos(angle), radius * std::sin(angle), z);
    }
    glEnd();
}
}

namespace RenderGeometry
{
void SolidCube(float size)
{
    const float half = size * .5f;
    const Vertex3 corners[] = {{-half, -half, -half},
                               {half, -half, -half},
                               {half, half, -half},
                               {-half, half, -half},
                               {-half, -half, half},
                               {half, -half, half},
                               {half, half, half},
                               {-half, half, half}};
    const int faces[][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
                            {3, 7, 6, 2}, {0, 4, 7, 3}, {1, 2, 6, 5}};
    const Vertex3 normals[] = {{0, 0, -1}, {0, 0, 1}, {0, -1, 0},
                               {0, 1, 0},  {-1, 0, 0}, {1, 0, 0}};

    glBegin(GL_QUADS);
    for (int face = 0; face < 6; ++face)
    {
        glNormal3f(normals[face].x, normals[face].y, normals[face].z);
        for (int corner : faces[face])
            glVertex3f(corners[corner].x, corners[corner].y, corners[corner].z);
    }
    glEnd();
}

void SolidCylinder(float radius, float height, int slices, int stacks)
{
    slices = std::max(3, slices);
    stacks = std::max(1, stacks);
    for (int stack = 0; stack < stacks; ++stack)
    {
        const float low = height * float(stack) / float(stacks);
        const float high = height * float(stack + 1) / float(stacks);
        glBegin(GL_QUADS);
        for (int slice = 0; slice < slices; ++slice)
        {
            const float a = 2 * Pi * float(slice) / float(slices);
            const float b = 2 * Pi * float(slice + 1) / float(slices);
            const float ax = std::cos(a), ay = std::sin(a);
            const float bx = std::cos(b), by = std::sin(b);
            glNormal3f(ax, ay, 0);
            glVertex3f(radius * ax, radius * ay, low);
            glNormal3f(bx, by, 0);
            glVertex3f(radius * bx, radius * by, low);
            glVertex3f(radius * bx, radius * by, high);
            glNormal3f(ax, ay, 0);
            glVertex3f(radius * ax, radius * ay, high);
        }
        glEnd();
    }
    Disk(radius, slices, 0, -1, true);
    Disk(radius, slices, height, 1, false);
}

void SolidCone(float baseRadius, float height, int slices, int stacks)
{
    slices = std::max(3, slices);
    stacks = std::max(1, stacks);
    for (int stack = 0; stack < stacks; ++stack)
    {
        const float lowZ = height * float(stack) / float(stacks);
        const float highZ = height * float(stack + 1) / float(stacks);
        const float lowRadius = baseRadius * (1 - float(stack) / float(stacks));
        const float highRadius = baseRadius * (1 - float(stack + 1) / float(stacks));
        glBegin(GL_QUADS);
        for (int slice = 0; slice < slices; ++slice)
        {
            const float a = 2 * Pi * float(slice) / float(slices);
            const float b = 2 * Pi * float(slice + 1) / float(slices);
            const float ax = std::cos(a), ay = std::sin(a);
            const float bx = std::cos(b), by = std::sin(b);
            const float normalZ = baseRadius / std::max(height, .0001f);
            glNormal3f(ax, ay, normalZ);
            glVertex3f(lowRadius * ax, lowRadius * ay, lowZ);
            glNormal3f(bx, by, normalZ);
            glVertex3f(lowRadius * bx, lowRadius * by, lowZ);
            glVertex3f(highRadius * bx, highRadius * by, highZ);
            glNormal3f(ax, ay, normalZ);
            glVertex3f(highRadius * ax, highRadius * ay, highZ);
        }
        glEnd();
    }
    Disk(baseRadius, slices, 0, -1, true);
}

void SolidIcosahedron()
{
    constexpr float x = .5257311121f;
    constexpr float z = .8506508084f;
    const Vertex3 vertices[] = {{-x, 0, z}, {x, 0, z},   {-x, 0, -z}, {x, 0, -z},
                                {0, z, x},  {0, z, -x},  {0, -z, x},  {0, -z, -x},
                                {z, x, 0},  {-z, x, 0},  {z, -x, 0},  {-z, -x, 0}};
    const int faces[][3] = {{0, 4, 1},  {0, 9, 4},  {9, 5, 4},  {4, 5, 8},  {4, 8, 1},
                            {8, 10, 1}, {8, 3, 10}, {5, 3, 8},  {5, 2, 3},  {2, 7, 3},
                            {7, 10, 3}, {7, 6, 10}, {7, 11, 6}, {11, 0, 6}, {0, 1, 6},
                            {6, 1, 10}, {9, 0, 11}, {9, 11, 2}, {9, 2, 5},  {7, 2, 11}};

    glBegin(GL_TRIANGLES);
    for (const auto &face : faces)
        for (int index : face)
        {
            const auto &vertex = vertices[index];
            glNormal3f(vertex.x, vertex.y, vertex.z);
            glVertex3f(vertex.x, vertex.y, vertex.z);
        }
    glEnd();
}
}
