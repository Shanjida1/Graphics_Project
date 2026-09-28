#pragma once

// Sphere parameters are separated like the course sample.
// The immediate-mode sphere drawing function is implemented in main.cpp
// to keep the project self-contained and one-click buildable.
struct SphereSettings
{
    int stacks;
    int slices;

    SphereSettings(int stackCount = 14, int sliceCount = 20)
        : stacks(stackCount), slices(sliceCount)
    {
    }
};
