#pragma once

#include "CoreMinimal.h"
#include "ScriptModel.h"
#include "GAS_ShotListTypes.h"

struct FGASScript;

// ============================================================
// LEGACY Shot List Builder (Scene-only)
// Used by existing UI (SGAS_*)
// ============================================================

bool BuildShotListFromJson(
    TArray<FGASShotDefinitionListRow>& OutRows
);

bool BuildShotListFromScript(
    const FGASScript& Script,
    const FGASSceneNumberingSettings& SceneNumbering,
    TArray<FGASShotDefinitionListRow>& OutRows
);

float GetShotDistance(EGASShotType ShotType);

// ============================================================
// FUTURE Shot List Builder (Scene + Shots)
// (NOT wired yet)
// ============================================================

// bool BuildShotListFromScriptV2(
//     const FGASScript& Script,
//     const FGASSceneNumberingSettings& SceneNumbering,
//     TArray<FGASShotListSceneRow>& OutScenes
// );
