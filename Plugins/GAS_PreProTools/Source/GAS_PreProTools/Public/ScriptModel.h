#pragma once

#include "CoreMinimal.h"
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

    // -------------------------------------------------
    // OPTION A: Scene-derived shot support
    // -------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDerivedFromScene = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ShotId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlockId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CharOffset = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LineIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Notes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color = FLinearColor::Yellow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Metadata;

    FGASMarker() {}

    bool operator==(const FGASMarker& Other) const
    {
        return Id == Other.Id;
    }
};

UENUM(BlueprintType)
enum class EGASShotNumberingPolicy : uint8
{
    Numeric_1s UMETA(DisplayName = "1, 2, 3"),
    Numeric_10s UMETA(DisplayName = "10, 20, 30"),
    Alphabetic UMETA(DisplayName = "A, B, C (AA after Z)")
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
