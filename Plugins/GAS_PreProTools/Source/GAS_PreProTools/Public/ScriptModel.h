#pragma once

#include "CoreMinimal.h"
#include "GAS_NumberingEnums.h"
#include "GAS_ShotIntentTypes.h"
#include "Misc/Optional.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"


#include "ScriptModel.generated.h"


// ------------------------------------------------------------
// BLOCK TYPE ENUM
// ------------------------------------------------------------
UENUM(BlueprintType)
enum class EGASBlockType : uint8
{
    None            UMETA(DisplayName = "None"),
    SceneHeading    UMETA(DisplayName = "Scene Heading"),
    Action          UMETA(DisplayName = "Action"),
    Shot            UMETA(DisplayName = "Shot"),
    Character       UMETA(DisplayName = "Character"),
    Parenthetical   UMETA(DisplayName = "Parenthetical"),
    Dialogue        UMETA(DisplayName = "Dialogue"),
    Transition      UMETA(DisplayName = "Transition"),
    General         UMETA(DisplayName = "General")
};

// ------------------------------------------------------------
// MARKER TYPE ENUM
// ------------------------------------------------------------
UENUM(BlueprintType)
enum class EGASMarkerType : uint8
{
    ShotMarker      UMETA(DisplayName = "Shot Marker"),
    Comment         UMETA(DisplayName = "Comment"),
    Bookmark        UMETA(DisplayName = "Bookmark")
};


// ------------------------------------------------------------
// Shot Origin
// ------------------------------------------------------------
UENUM(BlueprintType)
enum class EGASShotOrigin : uint8
{
    NonBlocking UMETA(DisplayName = "NonBlocking"),
    Blocking    UMETA(DisplayName = "Blocking")
};

// ------------------------------------------------------------
// DUAL DIALOG
// ------------------------------------------------------------
UENUM(BlueprintType)
enum class EGASDualRole : uint8
{
    None,
    Left,
    Right
};

// ============================================================
// Shot Spatial State (replaces color-as-state)
// ============================================================

UENUM(BlueprintType)
enum class EGASShotSpatialState : uint8
{
    NoBlocking      UMETA(DisplayName = "No Blocking"),
    BlockingReady   UMETA(DisplayName = "Blocking Ready"),
    CameraPlaced    UMETA(DisplayName = "Camera Placed"),
    Locked          UMETA(DisplayName = "Locked")
};

FORCEINLINE FLinearColor GAS_SpatialStateToColor(EGASShotSpatialState InState)
{
    switch (InState)
    {
    case EGASShotSpatialState::NoBlocking:     return FLinearColor::Red;
    case EGASShotSpatialState::BlockingReady:
        return FLinearColor(0.2f, 0.55f, 1.0f, 1.0f);

        // Future (not implemented yet) — still give stable colors now.
    case EGASShotSpatialState::CameraPlaced:   return FLinearColor(0.f, 1.f, 1.f);
    case EGASShotSpatialState::Locked:         return FLinearColor::Green;
    default:                                   return FLinearColor::Red;
    }
}

// ------------------------------------------------------------
// SCRIPT BLOCK (REPLACES FScriptParagraph)
// ------------------------------------------------------------
USTRUCT(BlueprintType)
struct GAS_PREPROTOOLS_API FGASBlock
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASBlockType Type = EGASBlockType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString Speaker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    TArray<FString> CharacterExtensions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    float IndentLeft = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    float IndentRight = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    TMap<FString, FString> Metadata;

    UPROPERTY()
    FString BlockingLevelPath;

    UPROPERTY()
    FName AssignedSetId;

    // ------------------------------------------------------------
    // Scene-Level Cast (Derived)
    // ------------------------------------------------------------
    UPROPERTY()
    TArray<FString> CharactersInScene;



    // ------------------------------------------------------------
    // Dual Dialogue
    // ------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    bool bIsDualDialogue = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASDualRole DualRole = EGASDualRole::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    bool bIsExplicitDualDialogue = false;

    // -----------------------------------------------------
    // Scene Master Sequence (Rehearsal Timeline)
    // -----------------------------------------------------
    UPROPERTY()
    FString MasterSequencePath;



    FGASBlock() {}
};




// ------------------------------------------------------------
// SCRIPT MARKER
// ------------------------------------------------------------
USTRUCT(BlueprintType)
struct FGASMarker
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    FGuid MarkerGuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASMarkerType MarkerType = EGASMarkerType::ShotMarker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    EGASShotOrigin ShotOrigin = EGASShotOrigin::NonBlocking;

    UPROPERTY()
    int32 ShotIndex = 0;

    UPROPERTY()
    int32 ShotStartNumber = 1;

    UPROPERTY()
    int32 SceneShotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString BlockId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    int32 CharOffset = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    int32 LineIndex = -1;

    UPROPERTY()
    float ScreenX = -1.f;

    UPROPERTY()
    float ScreenY = -1.f;

    // Stored in SCRIPT SPACE (same coordinate system as ScreenY)
    UPROPERTY()
    float ShotLineStartY = -1.f;

    UPROPERTY()
    float ShotLineEndY = -1.f;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString Notes;

    // -----------------------------
    // Spatial state (authoritative)
    // -----------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    EGASShotSpatialState SpatialState = EGASShotSpatialState::NoBlocking;

    // -----------------------------
    // Visuals (derived; do NOT treat as state)
    // -----------------------------
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    FLinearColor Color = FLinearColor::Red;

    FORCEINLINE void RefreshDerivedColor()
    {
        Color = GAS_SpatialStateToColor(SpatialState);
    }

    FORCEINLINE void SetSpatialState(EGASShotSpatialState InState)
    {
        SpatialState = InState;
        RefreshDerivedColor();
    }

    FORCEINLINE void PromoteToCameraPlaced()
    {
        // Only allow promotion from BlockingReady
        if (SpatialState == EGASShotSpatialState::BlockingReady)
        {
            SetSpatialState(EGASShotSpatialState::CameraPlaced);
        }
    }

    FORCEINLINE void PromoteToLocked()
    {
        if (SpatialState == EGASShotSpatialState::CameraPlaced)
        {
            SetSpatialState(EGASShotSpatialState::Locked);
        }
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    TMap<FString, FString> Metadata;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    int32 ParagraphIndex = INDEX_NONE;


    FGASMarker()
        : MarkerGuid(FGuid::NewGuid())
    {
    }

    bool operator==(const FGASMarker& Other) const
    {
        return Id == Other.Id;
    }

    FORCEINLINE bool IsNonBlocking() const
    {
        return ShotOrigin == EGASShotOrigin::NonBlocking;
    }

    FORCEINLINE bool IsBlocking() const
    {
        return ShotOrigin == EGASShotOrigin::Blocking;
    }

    // --------------------------------------------------
    // Camera Data (One camera per shot)
    // --------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Camera")
    bool bHasCamera = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Camera")
    FTransform CameraTransform = FTransform::Identity;

    FORCEINLINE void BindCameraTransform(const FTransform& InTransform)
    {
        bHasCamera = true;
        CameraTransform = InTransform;

        if (IsBlocking())
        {
            PromoteToCameraPlaced();
        }
    }


    FString GetShotLabel(EGASShotNumberingPolicy Policy) const;
};


// ------------------------------------------------------------
// Shot Marker Authoritative Factory (Phase 1)
// ------------------------------------------------------------
struct GAS_PREPROTOOLS_API FGASShotMarkerFactory
{
    // Creates a ShotMarker ONLY if TargetBlockId resolves to a SceneHeading block.
    // Returns true on success and fills OutMarker.
    static bool CreateShotMarkerForSceneHeading(
        const TArray<FGASBlock>& Blocks,
        const FString& TargetBlockId,
        FGASMarker& OutMarker
    );
};


// ------------------------------------------------------------
// USER PAGE BREAK (AUTHORITATIVE)
// ------------------------------------------------------------
USTRUCT(BlueprintType)
struct FGASUserPageBreak
{
    GENERATED_BODY()

    // Page break occurs AFTER this block (stable identity)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString AfterBlockId;

    FGASUserPageBreak() {}

    bool operator==(const FGASUserPageBreak& Other) const
    {
        return AfterBlockId == Other.AfterBlockId;
    }
};

// ------------------------------------------------------------
// SCENE NUMBERING SETTINGS (SCRIPT-WIDE POLICY)
// ------------------------------------------------------------
UENUM(BlueprintType)
enum class EGASSceneNumberBaseStyle : uint8
{
    By10           UMETA(DisplayName = "By 10s"),
    By1            UMETA(DisplayName = "By 1s"),
    Alphabetical   UMETA(DisplayName = "Alphabetical")
};

USTRUCT(BlueprintType)
struct FGASSceneNumberingSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASSceneNumberBaseStyle BaseStyle = EGASSceneNumberBaseStyle::By10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASNumberingStyle Style = EGASNumberingStyle::FromScript;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Numbering")
    int32 StartNumber = 1;
};

// ------------------------------------------------------------
// SCENE NUMBER DISPLAY HELPERS (DECLARATIONS ONLY)
// ------------------------------------------------------------
namespace GASSceneNumbering
{
    GAS_PREPROTOOLS_API FString MakeSceneNumber(
        int32 ZeroBasedSceneIndex,
        EGASSceneNumberBaseStyle Style
    );
}

// ------------------------------------------------------------
// SHOT NUMBERING SETTINGS (SCENE-LOCAL POLICY)
// ------------------------------------------------------------
UENUM(BlueprintType)
enum class EGASShotNumberBaseStyle : uint8
{
    By1            UMETA(DisplayName = "By 1s"),
    By10           UMETA(DisplayName = "By 10s"),
    Alphabetical   UMETA(DisplayName = "Alphabetical")
};





// ------------------------------------------------------------
// Undo Snapshot (Authoritative Data Only)
// ------------------------------------------------------------
USTRUCT()
struct FGASScriptUndoSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FGASBlock> Blocks;

    UPROPERTY()
    TArray<FGASMarker> Markers;

    UPROPERTY()
    TArray<FGASUserPageBreak> UserPageBreaks;

    UPROPERTY()
    TMap<FString, FGASShotIntent> ShotIntents;
};

// ------------------------------------------------------------
// Character Registry Entry (Authoritative Identity)
// ------------------------------------------------------------
USTRUCT()
struct FGASCharacter
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid Id;

    UPROPERTY()
    FName Name;
};


USTRUCT()
struct FGASShotCastMember
{
    GENERATED_BODY()

    UPROPERTY()
    FString CharacterName;

    UPROPERTY()
    bool bEnabled = true;
};



// ------------------------------------------------------------
// Script-Level Character Definition
// ------------------------------------------------------------
USTRUCT(BlueprintType)
struct FGASCharacterDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    FString CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    FString DefaultMeshPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    bool bEnabled;

    FString GetDefaultMeshName() const
    {
        return FPackageName::GetShortName(DefaultMeshPath);
    }

    FGASCharacterDefinition()
        : CharacterName(TEXT(""))
        , DefaultMeshPath(TEXT(""))
        , bEnabled(true)
    {
    }
};

USTRUCT(BlueprintType)
struct FGASProjectSettings
{
    GENERATED_BODY()

    // ------------------------------------------------------------
    // Project Display
    // ------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    FString AspectRatio = TEXT("16:9");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    EGASProjectFrameRate FrameRate =
        EGASProjectFrameRate::FPS_24;

    // ------------------------------------------------------------
    // Scene Numbering
    // ------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    EGASNumberingStyle SceneNumberingStyle =
        EGASNumberingStyle::CountBy10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    int32 SceneStartNumber = 10;

    // ------------------------------------------------------------
    // Shot Numbering
    // ------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    EGASShotNumberingPolicy ShotNumberingPolicy =
        EGASShotNumberingPolicy::Numeric_10s;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    int32 ShotStartNumber = 10;

    // ------------------------------------------------------------
    // Project Behavior
    // ------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    bool bEnableBlocking = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    bool bEnableShotLayers = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    bool bAutoCreateMasterSequence = true;
};

// ------------------------------------------------------------
// FULL SCRIPT CONTAINER
// ------------------------------------------------------------
USTRUCT(BlueprintType)
struct GAS_PREPROTOOLS_API FGASScript
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FString Uuid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    FGASProjectSettings ProjectSettings;
    // ------------------------------------------------------------
    // Scene Numbering Settings (Authoritative)
    // ------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    FGASSceneNumberingSettings SceneNumbering;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Numbering")
    int32 StartNumber = 1;

    // ------------------------------------------------------------
    // Shot Numbering Policy (Authoritative)
    // ------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    EGASShotNumberingPolicy ShotNumberingPolicy =
        EGASShotNumberingPolicy::Numeric_1s;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    TArray<FGASBlock> Blocks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS|Numbering")
    int32 ShotStartNumber = 1;

    // ------------------------------------------------------------
    // Script-Level Cast (Authoritative Character Definitions)
    // ------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
    TArray<FGASCharacterDefinition> Cast;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    TArray<FGASMarker> Markers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    TArray<FGASUserPageBreak> UserPageBreaks;

    // =====================================================
    // Shot Intent (Creative Meaning)
    // Keyed by FGASMarker::Id
    // =====================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GAS")
    TMap<FString, FGASShotIntent> ShotIntents;


    // ------------------------------------------------------------
    // Character Registry (authoritative, stable IDs)
    // Derived from Character blocks via sync
    // ------------------------------------------------------------
    UPROPERTY()
    TArray<FGASCharacter> Characters;


    void RegisterCharactersFromBlocks();

    void EnsureScriptCastDefinitionsExist();
    void RebuildSceneCharacterLists();

    FGASCharacterDefinition* FindCharacterDefinition(const FString& CharacterName);
    const FGASCharacterDefinition* FindCharacterDefinition(const FString& CharacterName) const;

    // ------------------------------------------------------------
    // Undo / Redo Stacks (Authoritative)
    // ------------------------------------------------------------
    UPROPERTY()
    TArray<FGASScriptUndoSnapshot> UndoStack;

    UPROPERTY()
    TArray<FGASScriptUndoSnapshot> RedoStack;

    // ------------------------------------------------------------
    // Undo Snapshot Capture
    // ------------------------------------------------------------
    void CaptureUndoSnapshot();

    // ------------------------------------------------------------
    // Undo / Redo Execution
    // ------------------------------------------------------------
    bool CanUndo() const;
    bool CanRedo() const;

    void Undo();
    void Redo();

    // ------------------------------------------------------------
    // Post-mutation sync (authoritative)
    // ------------------------------------------------------------
    void PostScriptMutation();


    FGASScript() {}
};
// ------------------------------------------------------------
// Shot number display helper (shared, UI-safe)
// ------------------------------------------------------------
GAS_PREPROTOOLS_API FString ResolveShotDisplayLabel(
    const FGASScript& Script,
    int32 SceneBlockIndex,
    int32 ShotIndexZeroBased
);


