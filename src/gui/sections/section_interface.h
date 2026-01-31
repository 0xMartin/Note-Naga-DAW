#pragma once

/**
 * @brief Interface for application sections that can be activated/deactivated.
 * 
 * All main sections (MidiEditorSection, DSPEditorSection, MediaExportSection)
 * should implement this interface to properly manage resources when switching
 * between sections.
 * 
 * When a section is deactivated:
 * - Heavy background processes should be stopped (e.g., preview workers, timers)
 * - Real-time visualizations should be disabled (e.g., spectrum analyzer)
 * - Animations should be paused
 * 
 * When a section is activated:
 * - Background processes should be restarted
 * - Visualizations should be re-enabled
 * - UI should refresh to current state
 */
class ISection {
public:
    virtual ~ISection() = default;
    
    /**
     * @brief Called when section becomes visible and active.
     * 
     * Implementations should:
     * - Start any background workers/threads
     * - Enable real-time visualizations
     * - Refresh UI to current data state
     */
    virtual void onSectionActivated() = 0;
    
    /**
     * @brief Called when section becomes hidden/inactive.
     * 
     * Implementations should:
     * - Stop background workers to save CPU/memory
     * - Disable heavy visualizations
     * - Pause any timers or animations
     */
    virtual void onSectionDeactivated() = 0;
};
