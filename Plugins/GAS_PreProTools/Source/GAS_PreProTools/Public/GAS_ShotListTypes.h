#pragma once

#include "CoreMinimal.h"
#include "GAS_ShotListTypes.generated.h"

// ============================================================
// Scene Heading Classification
// ============================================================

UENUM(BlueprintType)
enum class EGASSceneHeadingKind : uint8
{
    Location        UMETA(DisplayName = "Location"),
    Transition      UMETA(DisplayName = "Transition"),
    Structural      UMETA(DisplayName = "Structural"),
    Montage         UMETA(DisplayName = "Montage"),
    Unknown         UMETA(DisplayName = "Unknown")
};


// ============================================================
// Shot List – Shot Row
// ============================================================

USTRUCT()
struct FGASShotListShotRow
{
    GENERATED_BODY()

    // -----------------------------
    // Identity
    // -----------------------------
    UPROPERTY()
    FGuid ShotId;

    // AUTHORITATIVE shot number (10, 20, 30, etc)
    UPROPERTY()
    int32 DisplayShotNumber = 0;

    UPROPERTY()
    FString ShotNumber;

    // -----------------------------
    // Script-derived
    // -----------------------------
    UPROPERTY()
    FString ShotType;             // LS, CU, OTS, etc (from marker)

    UPROPERTY()
    int32 LengthEighths = 0;      // Marker → tail (script-driven)

    UPROPERTY()
    FString Lens;                 // Marker-derived default

    UPROPERTY()
    FString Camera;

    UPROPERTY()
    bool bCameraMoves = false;

    // -----------------------------
    // Shot List overrides / user data
    // -----------------------------
    UPROPERTY()
    FString LensOverride;         // Optional override (shot list wins)

    UPROPERTY()
    FString Description;          // User-entered

    UPROPERTY()
    FString Notes;                // User-entered

    UPROPERTY()
    bool bNotesExpanded = false;


    // -----------------------------
    // Shot tools / status (dropdown mask)
    // -----------------------------
    UPROPERTY()
    uint32 ShotOptionsMask = 0;

    // -----------------------------
    // Ordering
    // -----------------------------
    UPROPERTY()
    int32 ScriptOrderIndex = 0;   // Absolute order in script
};


// ============================================================
// Shot List – Scene Row
// ============================================================

USTRUCT()
struct FGASShotListSceneRow
{
    GENERATED_BODY()

    // -----------------------------
    // Identity
    // -----------------------------
    UPROPERTY()
    FString SceneId;              // Stable internal ID

    UPROPERTY()
    FString SceneNumber;          // User-imported (immutable)

    // -----------------------------
    // Script-derived
    // -----------------------------
    UPROPERTY()
    int32 StartPage = INDEX_NONE; // Page where scene STARTS

    UPROPERTY()
    FString SceneHeading;         // Full scene heading text

    UPROPERTY()
    EGASSceneHeadingKind SceneKind = EGASSceneHeadingKind::Unknown;

    // -----------------------------
    // Length
    // -----------------------------
    UPROPERTY()
    int32 LengthEighths = 0;      // Scene span in eighths

    UPROPERTY()
    FString FormattedLength;

    // -----------------------------
    // Shot List overrides / user data
    // -----------------------------
    UPROPERTY()
    FString TimeOfDayOverride;    // User-entered, does not affect script

    UPROPERTY()
    FString SetId;                // Placeholder for future Set system

    // -----------------------------
    // Children
    // -----------------------------
    UPROPERTY()
    TArray<FGASShotListShotRow> Shots;
};


// ============================================================
// Shot List – CSV Export Row (Flat, Derived)
// ============================================================

USTRUCT()
struct FGASShotListCSVRow
{
    GENERATED_BODY()

    FString SceneNumber;
    FString ShotNumber;
    FString ShotType;

    int32   PageStart = 0;
    int32   LengthEighths = 0;

    FString TimeOfDay;
    FString Set;

    FString Lens;
    FString Camera;
    FString Description;
    FString Notes;
};

// ============================================================
// LEGACY: Scene List Row (TRANSITIONAL)
// Used by GAS_ShotListBuilder.cpp (scene-only v1)
// Will be retired once ShotListBuilder is complete.
// ============================================================

USTRUCT()
struct FGASShotDefinitionListRow
{
    GENERATED_BODY()

    // Row type
    UPROPERTY()
    bool bIsShotRow = false;

    // Script page number
    UPROPERTY()
    int32 PageNumber = INDEX_NONE;

    // Scene linkage
    UPROPERTY()
    int32 SceneBlockIndex = INDEX_NONE;

    UPROPERTY()
    FString SceneId; // V2 authoritative scene id

    // Scene data
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

    // Shot-only placeholders (unused here)
    UPROPERTY()
    FString MarkerId;

    UPROPERTY()
    FString DisplayName;
};

// ============================================================
// Utilities
// ============================================================

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


