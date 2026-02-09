#pragma once

#include <string>

// Strategy Pattern: Interface for different lighting models
// Each lighting model knows which shaders to use
class ILightingModel {
public:
    virtual ~ILightingModel() = default;

    // Returns the fragment shader path for this lighting model
    virtual std::string getFragmentShaderPath() const = 0;

    // Returns the name of this lighting model (for UI)
    virtual const char* getName() const = 0;
};
