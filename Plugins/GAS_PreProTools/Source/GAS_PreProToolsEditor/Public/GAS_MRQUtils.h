#pragma once

#include "CoreMinimal.h"

class ULevelSequence;
class UGASPreProProject;

// Forward declares — keep MRQ headers out of public includes
class UMoviePipelineQueue;
class UMoviePipelineExecutorJob;
class UMoviePipelinePrimaryConfig;

/**
 * GAS_MRQUtils
 *
 * Static helper for Movie Render Queue integration.
 * Responsible for:
 *   - Deriving the film master sequence path from the active project
 *   - Building and submitting an MRQ job pointed at that sequence
 *   - Opening the MRQ window with the job pre-configured
 *
 * Never owns state. Always driven by the active project + script.
 */
class GAS_PREPROTOOLSEDITOR_API FGAS_MRQUtils
{
public:

    /**
     * Main entry point.
     * Derives the film master sequence from ProjectName,
     * creates an MRQ job with correct output path,
     * and opens the MRQ window for director confirmation.
     *
     * @param Project        The active GAS project asset
     * @return true if job was successfully created and window opened
     */
    static bool SendToRenderQueue(UGASPreProProject* Project);

    /**
     * Derives the film-level master sequence path from project name.
     * Convention: /Game/PrePro/Projects/{ProjectName}/Sequences/{ProjectName}_Master
     *
     * @param ProjectName    Clean project name (no spaces)
     * @return Full UE asset path string
     */
    static FString GetFilmMasterSequencePath(const FString& ProjectName);

    /**
     * Derives the render output directory for this project.
     * Convention: {ProjectContentDir}/PrePro/Projects/{ProjectName}/Movies/
     *
     * @param ProjectName    Clean project name (no spaces)
     * @return Absolute filesystem path
     */
    static FString GetRenderOutputPath(const FString& ProjectName);

private:

    // Non-instantiable
    FGAS_MRQUtils() = delete;
};