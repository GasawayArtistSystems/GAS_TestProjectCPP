#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

// IMPORTANT: Full definitions required for UPROPERTY
#include "ScriptModel.h"

#include "GASScriptAsset.generated.h"



UCLASS(BlueprintType)
class GAS_PREPROTOOLS_API UGASScriptAsset : public UObject
{
    GENERATED_BODY()

public:

    // =========================================================
    // Script Data (Serialized by UE)
    // =========================================================

    UPROPERTY(EditAnywhere, Category = "Script")
    TArray<FGASBlock> Blocks;

    UPROPERTY(EditAnywhere, Category = "Script")
    TArray<FGASUserPageBreak> UserPageBreaks;

    // Optional: remember where this was imported from
    UPROPERTY(EditAnywhere, Category = "Script")
    FString SourceJsonPath;
};
