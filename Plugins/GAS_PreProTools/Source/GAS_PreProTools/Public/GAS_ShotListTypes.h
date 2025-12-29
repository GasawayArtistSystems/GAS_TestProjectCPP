#pragma once

#include "CoreMinimal.h"
#include "GAS_ShotListTypes.generated.h"


UENUM(BlueprintType)
enum class EGASSceneHeadingKind : uint8
{
    Location        UMETA(DisplayName = "Location"),
    Transition      UMETA(DisplayName = "Transition"),
    Structural      UMETA(DisplayName = "Structural"),
    Montage         UMETA(DisplayName = "Montage"),
    Unknown         UMETA(DisplayName = "Unknown")
};



USTRUCT()
struct FGASShotDefinitionListRow
{
    GENERATED_BODY()

    // -----------------------------
    // Row type
    // -----------------------------
    UPROPERTY()
    bool bIsShotRow = false;

    // -----------------------------
    // Script page number (scene rows)
    // -----------------------------
    UPROPERTY()
    int32 PageNumber = INDEX_NONE;

    // -----------------------------
    // Scene linkage (both scene + shot rows)
    // -----------------------------
    UPROPERTY()
    int32 SceneBlockIndex = INDEX_NONE;

    // -----------------------------
    // Scene-only data
    // -----------------------------
    UPROPERTY()
    FString SceneNumber;

    UPROPERTY()
    EGASSceneHeadingKind SceneKind = EGASSceneHeadingKind::Unknown;

    UPROPERTY()
    FString IntExt;

    UPROPERTY()
    FString SceneTitle;

    UPROPERTY()
    FString TimeOfDay;

    UPROPERTY()
    FString Set;

    // -----------------------------
    // Shot-only data (Option A)
    // -----------------------------
    UPROPERTY()
    FString MarkerId;

    UPROPERTY()
    FString DisplayName;

    UPROPERTY()
    bool bDerivedFromScene = false;
};


inline EGASSceneHeadingKind ClassifySceneHeading(const FString& InText)
{
    FString T = InText.ToUpper().TrimStartAndEnd();

    if (T.StartsWith(TEXT("INT.")) || T.StartsWith(TEXT("EXT.")))
    {
        return EGASSceneHeadingKind::Location;
    }

    if (T.StartsWith(TEXT("CUT TO")) ||
        T.StartsWith(TEXT("FADE")) ||
        T.StartsWith(TEXT("DISSOLVE")))
    {
        return EGASSceneHeadingKind::Transition;
    }

    static const TArray<FString> StructuralKeywords =
    {
        TEXT("BLACK SCREEN"),
        TEXT("END FLASHBACK"),
        TEXT("END DREAM"),
        TEXT("FLASHBACK"),
        TEXT("FLASH FORWARD")
    };

    for (const FString& Keyword : StructuralKeywords)
    {
        if (T.Contains(Keyword))
        {
            return EGASSceneHeadingKind::Structural;
        }
    }

    return EGASSceneHeadingKind::Unknown;
}
