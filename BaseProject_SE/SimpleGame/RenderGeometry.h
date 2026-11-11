#pragma once

namespace RenderGeometry
{
void SolidCube(float size);
void SolidCylinder(float radius, float height, int slices, int stacks);
void SolidCone(float baseRadius, float height, int slices, int stacks);
void SolidIcosahedron();
}
