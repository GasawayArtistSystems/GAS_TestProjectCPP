#pragma once

#include "CoreMinimal.h"
#include "ScriptModel.h"
#include "GASPreProProject.h"
#include "ScriptFormatRules.h"
#include "ScriptLayoutEngine.generated.h"

USTRUCT()
struct FRenderedParagraph
{
    GENERATED_BODY()

    // Which original FGASBlock this paragraph belongs to
    UPROPERTY()
    FString BlockId;

    // Paragraph index in the rendered sequence
    UPROPERTY()
    int32 ParagraphIndex = -1;

    // What type of block this is (Action, Dialogue, etc.)
    UPROPERTY()
    EGASBlockType BlockType = EGASBlockType::None;

    // Wrapped lines
    UPROPERTY()
    TArray<FString> Lines;

    // Position + size
    UPROPERTY()
    float TopY = 0.f;

    UPROPERTY()
    float Height = 0.f;

    // Indentation
    UPROPERTY()
    float IndentLeft = 0.f;

    UPROPERTY()
    float IndentRight = 0.f;

    // (Optional future use)
    UPROPERTY()
    FText SourceText;

    UPROPERTY()
    TMap<FString, FString> SourceMetadata;

    // ================================
    // NEW: Page break + pagination data
    // ================================

    // The page this paragraph belongs to (1 = first page)
    UPROPERTY()
    int32 PageNumber = 1;

    // True if this paragraph begins a new page
    UPROPERTY()
    bool bStartsPage = false;

    // True if the user explicitly forced a page break here
    UPROPERTY()
    bool bUserForcedPageBreak = false;

    UPROPERTY()
    bool bAutoBreakCausedHere = false;

};



class ScriptLayoutEngine
{
public:

    static TArray<FRenderedParagraph> LayoutScript(
        const TArray<FGASBlock>& Blocks,
        float PanelWidth,
        const TArray<int32>& UserPageBreaks,
        const TArray<FString>& SuppressedAutoBreakBlockIds
    );




    // backward compatibility for older calls
    static TArray<FRenderedParagraph> LayoutScript(
        const TArray<FGASBlock>& Blocks,
        float PanelWidth
    );


private:
    // Formatting helpers
    static float GetLeftIndent(EGASBlockType Type);
    static float GetRightIndent(EGASBlockType Type);
    static float GetAvailableWidth(EGASBlockType Type);

    static float ComputeParagraphHeight(
        const FString& Text,
        float IndentLeft,
        float IndentRight,
        const FSlateFontInfo& FontInfo);

    static bool BlocksMustStayTogether(EGASBlockType A, EGASBlockType B);

}
;
