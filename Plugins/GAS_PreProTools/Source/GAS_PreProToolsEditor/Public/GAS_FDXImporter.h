#pragma once

#include "CoreMinimal.h"
#include "GASPreProProject.h"     // For FGASBlock, EGASBlockType
#include "GAS_FDXImporter.generated.h"

struct FRawFDXBlock
{
    FString Xml;

    // This paragraph CONTAINS a <DualDialogue> block
    bool bIsDualWrapper = false;

    // This paragraph is INSIDE a <DualDialogue> block
    bool bInsideDualDialogue = false;
};



UCLASS()
class GAS_PREPROTOOLSEDITOR_API UGAS_FDXImporter : public UObject
{
    GENERATED_BODY()

public:

    // NEW — returns a full FGASScript containing Blocks + Scenes + Markers
    UFUNCTION(BlueprintCallable, Category = "FDX")
    static bool ImportFDXToScript(const FString& FilePath, FGASScript& OutScript);

    void PostProcessDualDialogue(TArray<FGASBlock>& Blocks);

private:

    // Extract <Paragraph> elements from xml text
    static void ExtractRawParagraphBlocks(
        const FString& XmlText,
        TArray<FRawFDXBlock>& OutBlocks,
        bool bIsRecursive = false);


    static bool ConvertFDXBlockToGAS(
        const FString& Block,
        FGASBlock& OutBlock
    );

    // Map FDX paragraph type (Action, Dialogue, Scene Heading, etc.)
    static EGASBlockType MapFDXType(const FString& TypeString);

    static void GetFDXSections(
        const FString& XmlText,
        int32& OutTitlePageStart,
        int32& OutTitlePageEnd,
        int32& OutContentStart,
        int32& OutContentEnd);



};
