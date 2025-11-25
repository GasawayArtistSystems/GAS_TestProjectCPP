#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GASPreProProject.generated.h"

/*-----------------------------------------
    SCRIPT BLOCK
------------------------------------------*/

UENUM()
enum class EGASBlockType : uint8
{
    Action,
    Dialogue,
    Parenthetical,
    SceneHeading,
    Transition,
    General
};

USTRUCT(BlueprintType)
struct FGASScriptBlock
{
    GENERATED_BODY();

    // Stable ID for this block
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid BlockID;

    // The text content
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Text;

    // Parsed type (Action, Dialogue, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASBlockType BlockType = EGASBlockType::General;

    // Internal: Scene this block belongs to
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid SceneID;

    FGASScriptBlock()
    {
        BlockID = FGuid::NewGuid();
    }
};

/*-----------------------------------------
    SCENE
------------------------------------------*/

USTRUCT(BlueprintType)
struct FGASScene
{
    GENERATED_BODY();

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid SceneID;

    // User-facing label such as: "010", "20", "3", "EXT1"
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SceneLabel;

    // Raw scene heading text from script
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString HeadingText;

    // List of script blocks that belong to this scene
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> BlockIDs;

    // Shots that START in this scene
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> ShotIDs;

    FGASScene()
    {
        SceneID = FGuid::NewGuid();
    }
};

/*-----------------------------------------
    SHOT
------------------------------------------*/

USTRUCT(BlueprintType)
struct FGASShot
{
    GENERATED_BODY();

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ShotID;

    // Label such as "010", "20", "A", "20B"
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ShotLabel;

    // MS / CU / Insert / etc.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ShotType;

    // User notes
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Notes;

    FGASShot()
    {
        ShotID = FGuid::NewGuid();
    }
};

/*-----------------------------------------
    SHOT MARKER
------------------------------------------*/

USTRUCT(BlueprintType)
struct FGASShotMarker
{
    GENERATED_BODY();

    // Which shot this marker represents
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ShotID;

    // Covers a range in the script
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid StartBlockID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid EndBlockID;

    // Optional metadata for overlay UI
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Notes;
};

/*-----------------------------------------
    PROJECT ASSET
------------------------------------------*/

UCLASS(BlueprintType)
class GAS_PREPROTOOLSEDITOR_API UGASPreProProject : public UDataAsset
{
    GENERATED_BODY()

public:

    /*-----------------------------------------
        SCRIPT STORAGE
    ------------------------------------------*/

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Script")
    TArray<FGASScriptBlock> ScriptBlocks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Script")
    TArray<FGASScene> Scenes;

    /*-----------------------------------------
        SHOTS + MARKERS
    ------------------------------------------*/

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shots")
    TArray<FGASShot> Shots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shots")
    TArray<FGASShotMarker> ShotMarkers;

public:

    /*-----------------------------------------
        ADDERS
    ------------------------------------------*/

    FGuid AddScene(const FString& Label, const FString& Heading);

    FGuid AddShot(const FString& Label);

    void AddShotMarker(
        const FGuid& ShotID,
        const FGuid& StartBlockID,
        const FGuid& EndBlockID,
        const FString& Notes = TEXT("")
    );
};
