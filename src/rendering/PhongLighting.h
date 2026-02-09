#pragma once

#include "ILightingModel.h"

// Phong Lighting Model: Ambient + Diffuse + Specular (reflection vector)
class PhongLighting : public ILightingModel {
public:
    std::string getFragmentShaderPath() const override {
        return std::string(SHADER_DIR) + "/shader_phong.frag.spv";
    }

    const char* getName() const override {
        return "Phong";
    }
};
