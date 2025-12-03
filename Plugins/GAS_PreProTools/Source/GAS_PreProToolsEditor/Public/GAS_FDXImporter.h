#pragma once

#include "CoreMinimal.h"
#include "GAS_FDXImporter.generated.h"

UENUM(BlueprintType)
enum class EParagraphType : uint8
{
    Unknown = 0,
    SceneHeading,
    Action,
    Character,
    Dialogue,
    Parenthetical,
    Transition
};

USTRUCT(BlueprintType)
struct GAS_PREPROTOOLSEDITOR_API FScriptParagraph
{
    GENERATED_BODY()

    UPROPERTY()
    FString Text;

    UPROPERTY()
    FString TypeString;

    UPROPERTY()
    EParagraphType ParagraphType = EParagraphType::Unknown;

    // TEMP legacy compatibility
    UPROPERTY()
    FString Type;
};

class GAS_PREPROTOOLSEDITOR_API GAS_FDXImporter
{
public:
    static bool ImportFDX(const FString& FilePath, TArray<FScriptParagraph>& OutParagraphs);

private:
    static void ExtractRawParagraphBlocks(const FString& XmlText, TArray<FString>& OutBlocks);
    static bool ParseParagraphBlock(const FString& Block, FScriptParagraph& OutPara);
    static EParagraphType MapFDXType(const FString& TypeString);
};
