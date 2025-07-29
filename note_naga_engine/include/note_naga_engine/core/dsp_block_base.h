#pragma once
#include <string>
#include <vector>
#include <variant>

/**
 * @brief Parameter types for DSP blocks.
 */
enum class DSPParamType {
    Float,
    Int,
    Bool
};

#define DSP_UI_TYPE_SLIDER_VERTICAL "slider_vertical"
#define DSP_UI_TYPE_DIAL "dial"
#define DSP_UI_TYPE_DIAL_CENTERED "dial_centered"

/**
 * @brief Structure for describing DSP block parameters.
 */
struct DSPParamDescriptor {
    std::string name;
    DSPParamType type;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    std::string uiType; 
};

/**
 * @brief Base class for DSP blocks in the Note Naga engine.
 * This class defines the interface for processing audio data,
 * managing parameters, and activation state.
 * Derived classes should implement the process method and parameter management.
 */
class NoteNagaDSPBlockBase {
public:
    virtual ~NoteNagaDSPBlockBase() {}

    /**
     * @brief Process audio data (in-place, mono and stereo).
     */
    virtual void process(float* left, float* right, size_t numFrames) = 0;

    /**
     * @brief Get parameter descriptors for the UI.
     */
    virtual std::vector<DSPParamDescriptor> getParamDescriptors() = 0;

    /**
     * @brief Get the value of a parameter.
     */
    virtual float getParamValue(size_t idx) const = 0;

    /**
     * @brief Set the value of a parameter.
     */
    virtual void setParamValue(size_t idx, float value) = 0;

    /**
     * @brief Get the name of the DSP block.
     */
    void setActive(bool active) { active_ = active; }

    /**
     * @brief Check if the DSP block is active.
     */
    bool isActive() const { return active_; }

    /**
     * @brief Get the name of the DSP block.
     */
    virtual std::string getBlockName() const = 0;

private:
    bool active_ = true;
};