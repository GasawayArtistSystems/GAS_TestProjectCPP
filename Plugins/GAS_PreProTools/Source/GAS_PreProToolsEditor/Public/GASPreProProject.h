// File: GAS_PreProToolsEditor/Public/GASPreProProject.h
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
    Character,
    Transition,
    General
};

USTRUCT(BlueprintType)
struct FGASScriptBlock
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid BlockID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGASBlockType BlockType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid SceneID;

    // MUST be deterministic for the CDO; assign real GUIDs at creation sites.
    FGASScriptBlock()
        : BlockID(FGuid())           // zero GUID by default
        , Text(TEXT(""))
        , BlockType(EGASBlockType::General)
        , SceneID(FGuid())
    {
    }
};

/*-----------------------------------------
    SCENE
------------------------------------------*/

USTRUCT(BlueprintType)
struct FGASScene
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid SceneID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SceneLabel;     // e.g. "010", "20", "3", "EXT1"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString HeadingText;    // Raw scene heading

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> BlockIDs; // Blocks that belong to this scene

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> ShotIDs;  // Shots that START in this scene

    FGASScene()
        : SceneID(FGuid())          // zero GUID by default
        , SceneLabel(TEXT(""))
        , HeadingText(TEXT(""))
        , BlockIDs()
        , ShotIDs()
    {
    }
};

/*-----------------------------------------
    SHOT
------------------------------------------*/

USTRUCT(BlueprintType)
struct FGASShot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ShotID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ShotLabel;  // e.g. "010", "20", "A", "20B"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ShotType;   // MS / CU / Insert / etc.

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Notes;      // User notes

    FGASShot()
        : ShotID(FGuid())           // zero GUID by default
        , ShotLabel(TEXT(""))
        , ShotType(TEXT(""))
        , Notes(TEXT(""))
    {
    }
};

/*-----------------------------------------
    SHOT MARKER
------------------------------------------*/

USTRUCT(BlueprintType)
struct FGASShotMarker
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ShotID;           // Which shot this marker represents

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid StartBlockID;     // Range start in the script

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid EndBlockID;       // Range end in the script

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Notes;          // Optional metadata for overlay UI

    FGASShotMarker()
        : ShotID(FGuid())
        , StartBlockID(FGuid())
        , EndBlockID(FGuid())
        , Notes(TEXT(""))
    {
    }
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
