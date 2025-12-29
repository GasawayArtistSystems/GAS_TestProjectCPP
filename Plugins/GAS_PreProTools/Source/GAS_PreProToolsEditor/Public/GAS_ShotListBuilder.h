#pragma once
#include "CoreMinimal.h"
#include "ScriptModel.h"
#include "GAS_ShotListTypes.h"

struct FGASScript;

bool BuildShotListFromJson(TArray<FGASShotDefinitionListRow>& OutRows);

bool BuildShotListFromScript(
    const FGASScript& Script,
    const FGASSceneNumberingSettings& SceneNumbering,
    TArray<FGASShotDefinitionListRow>& OutRows
);
