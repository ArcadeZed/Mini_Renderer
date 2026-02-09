#pragma once

#include "ILightingModel.h"

// Blinn-Phong Lighting Model: Halfway vector instead of reflection
class BlinnPhongLighting : public ILightingModel {
public:
    std::string getFragmentShaderPath() const override {
        return std::string(SHADER_DIR) + "/shader_blinn_phong.frag.spv";
    }

    const char* getName() const override {
        return "Blinn-Phong";
    }
};
