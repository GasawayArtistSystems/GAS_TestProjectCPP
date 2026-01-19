#pragma once

#include "CoreMinimal.h"
#include "GAS_NumberingEnums.h"
#include "GAS_ShotIntentTypes.h"
#include "Misc/Optional.h"

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
    Derived UMETA(DisplayName = "Derived"),   // auto / implicit
    User    UMETA(DisplayName = "User")       // explicitly created by user
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

// ------------------------------------------------------------
// SCRIPT BLOCK (REPLACES FScriptParagraph)
// ------------------------------------------------------------
USTRUCT(BlueprintType)
struct FGASBlock
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASBlockType Type = EGASBlockType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Speaker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> CharacterExtensions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IndentLeft = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IndentRight = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Metadata;

    // ------------------------------------------------------------
    // Dual Dialogue
    // ------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsDualDialogue = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASDualRole DualRole = EGASDualRole::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsExplicitDualDialogue = false;

    FGASBlock() {}
};




// ------------------------------------------------------------
// SCRIPT MARKER
// ------------------------------------------------------------
USTRUCT(BlueprintType)
struct FGASMarker
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASMarkerType MarkerType = EGASMarkerType::ShotMarker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASShotOrigin ShotOrigin = EGASShotOrigin::Derived;

    UPROPERTY()
    int32 ShotIndex = 0;

    UPROPERTY()
    int32 SceneShotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlockId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CharOffset = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
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


    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Notes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Metadata;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ParagraphIndex = INDEX_NONE;


    FGASMarker() {}

    bool operator==(const FGASMarker& Other) const
    {
        return Id == Other.Id;
    }
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASSceneNumberBaseStyle BaseStyle = EGASSceneNumberBaseStyle::By10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASNumberingStyle Style = EGASNumberingStyle::FromScript;
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
// FULL SCRIPT CONTAINER
// ------------------------------------------------------------
USTRUCT(BlueprintType)
struct GAS_PREPROTOOLS_API FGASScript
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Uuid;

    // ------------------------------------------------------------
    // Scene Numbering Settings (Authoritative)
    // ------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGASSceneNumberingSettings SceneNumbering;

    // ------------------------------------------------------------
    // Shot Numbering Policy (Authoritative)
    // ------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASShotNumberingPolicy ShotNumberingPolicy =
        EGASShotNumberingPolicy::Numeric_1s;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGASBlock> Blocks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGASMarker> Markers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGASUserPageBreak> UserPageBreaks;

    // =====================================================
    // Shot Intent (Creative Meaning)
    // Keyed by FGASMarker::Id
    // =====================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FGASShotIntent> ShotIntents;


    UPROPERTY()
    TArray<FGASShotCastMember> ShotCast;


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


