#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ScriptModel.h"
#include "Data/GASShot.h"

struct FGASDerivedShotRow;

#include "GASPreProProject.generated.h"



/*
===============================================================================
 SCRIPT DATA MODEL
 ------------------------------------------------------------------------------
 These structs represent the authoritative script content.
 They are order-sensitive and form the backbone of the document.
===============================================================================
*/

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

    // Deterministic default for CDO
    FGASScriptBlock()
        : BlockID(FGuid())
        , Text(TEXT(""))
        , BlockType(EGASBlockType::General)
        , SceneID(FGuid())
    {
    }
};

/*
===============================================================================
 SCENE ORGANIZATION
 ------------------------------------------------------------------------------
 Scenes group script blocks and define structural boundaries.
===============================================================================
*/

USTRUCT(BlueprintType)
struct FGASScene
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid SceneID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SceneLabel;     // e.g. "010", "20", "EXT1"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString HeadingText;    // Raw scene heading text

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> BlockIDs; // Blocks belonging to this scene

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGuid> ShotIDs;  // Shots that START in this scene

    FGASScene()
        : SceneID(FGuid())
        , SceneLabel(TEXT(""))
        , HeadingText(TEXT(""))
    {
    }
};

/*
===============================================================================
 SHOTS + MARKERS
 ------------------------------------------------------------------------------
 Shots and markers are overlays that reference script blocks.
 They do NOT own script text.
===============================================================================
*/


USTRUCT(BlueprintType)
struct FGASShotDefinitionMarker
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid ShotID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid StartBlockID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid EndBlockID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Notes;

    FGASShotDefinitionMarker()
        : ShotID(FGuid())
        , StartBlockID(FGuid())
        , EndBlockID(FGuid())
        , Notes(TEXT(""))
    {
    }
};

/*
===============================================================================
 PROJECT (DOCUMENT)
 ------------------------------------------------------------------------------
 This asset represents an open Pre-Pro project.
 It owns the script, scenes, shots, and markers.
 Dirty state and save/load responsibility live here.
===============================================================================
*/

UCLASS(BlueprintType)
class GAS_PREPROTOOLSEDITOR_API UGASPreProProject : public UDataAsset
{
    GENERATED_BODY()

public:

    /*-----------------------------------------
        SCRIPT CONTENT
    ------------------------------------------*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Script")
    TArray<FGASScriptBlock> ScriptBlocks;



    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Script")
    TArray<FGASScene> Scenes;

    /*-----------------------------------------
        SHOTS + MARKERS
    ------------------------------------------*/
    UPROPERTY()
    TArray<FGASShotDefinition> ShotDefinitions;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shots")
    TArray<FGASShotDefinitionMarker> ShotMarkers;

    /*-----------------------------------------
        MUTATION API
        (These will mark the project dirty)
    ------------------------------------------*/
    FGuid AddScene(const FString& Label, const FString& Heading);
    FGuid AddShot();

    void AddShotMarker(
        const FGuid& ShotID,
        const FGuid& StartBlockID,
        const FGuid& EndBlockID,
        const FString& Notes = TEXT("")
    );

    /*-----------------------------------------
        DIRTY STATE API
    ------------------------------------------*/
    void MarkDirty();
    void ClearDirty();
    bool IsDirty() const;

    void BuildDerivedShotList(TArray<FGASDerivedShotRow>& OutShots) const;


    virtual void PostLoad() override;

private:

    /*-----------------------------------------
        DIRTY STATE
    ------------------------------------------*/
    bool bIsDirty = false;
};
