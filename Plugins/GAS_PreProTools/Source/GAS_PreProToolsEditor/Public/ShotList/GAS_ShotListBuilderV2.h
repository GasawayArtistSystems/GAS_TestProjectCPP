#pragma once

#include "CoreMinimal.h"
#include "ScriptModel.h"
#include "GAS_ShotListTypes.h"
#include "ScriptLayoutEngine.h"

// ============================================================
// Shot List Builder V2
// Scene + Shot hierarchical builder
// (Does NOT touch legacy FGASShotDefinitionListRow)
// ============================================================

struct FGASScript;

bool BuildShotListFromScriptV2(
    const FGASScript& Script,
    const FGASSceneNumberingSettings& SceneNumbering,
    const TArray<FRenderedParagraph>& RenderedParagraphs,
    TArray<FGASShotListSceneRow>& OutScenes
);

