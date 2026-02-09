#pragma once

#include "ILightingModel.h"

// PBR Lighting Model: Cook-Torrance with metalness/roughness
class PBRLighting : public ILightingModel {
public:
    std::string getFragmentShaderPath() const override {
        return std::string(SHADER_DIR) + "/shader_pbr.frag.spv";
    }

    const char* getName() const override {
        return "PBR (Cook-Torrance)";
    }
};
