#pragma once
#include <fstream>
#include <sstream>
#include <string>

// Course-style shader source helper.
// The bedroom application keeps the one-click Windows/OpenGL compatibility renderer,
// while these files preserve the same organized shader structure used in class projects.
struct ShaderSourcePair
{
    std::string vertexPath;
    std::string fragmentPath;

    ShaderSourcePair(const char* vertex, const char* fragment)
        : vertexPath(vertex), fragmentPath(fragment) {}

    static bool ReadTextFile(const std::string& path, std::string& out)
    {
        std::ifstream file(path, std::ios::in);
        if (!file.is_open()) return false;
        std::ostringstream stream;
        stream << file.rdbuf();
        out = stream.str();
        return !out.empty();
    }

    bool SourcesAvailable() const
    {
        std::string vertexSource, fragmentSource;
        return ReadTextFile(vertexPath, vertexSource) &&
               ReadTextFile(fragmentPath, fragmentSource);
    }
};

inline ShaderSourcePair BedroomBasicShader()
{
    return ShaderSourcePair("vertexShader.vs", "fragmentShader.fs");
}

inline ShaderSourcePair BedroomGouraudShader()
{
    return ShaderSourcePair("vertexShaderForGouraudShading.vs",
                            "fragmentShaderForGouraudShading.fs");
}

inline ShaderSourcePair BedroomPhongShader()
{
    return ShaderSourcePair("vertexShaderForPhongShading.vs",
                            "fragmentShaderForPhongShading.fs");
}
