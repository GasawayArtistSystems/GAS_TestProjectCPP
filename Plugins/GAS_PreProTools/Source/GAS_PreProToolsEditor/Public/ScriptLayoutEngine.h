#pragma once

#include "CoreMinimal.h"
#include "GAS_FDXImporter.h"
#include "GASPreProProject.h" 
#include "ScriptLayoutEngine.generated.h"


// ============================================================================
// Final Draft Formatting Constants (Shared across ScriptLayoutEngine + Panels)
// ============================================================================

static constexpr float GFD_LineHeight = 13.2f;

static constexpr float GFD_MarginLeft = 108.f;   // 1.5"
static constexpr float GFD_MarginRight = 72.f;    // 1"
static constexpr float GFD_MarginTop = 72.f;
static constexpr float GFD_MarginBottom = 72.f;

static constexpr float GFD_PageWidth = 612.f;   // 8.5"
static constexpr float GFD_PageHeight = 792.f;   // 11"


USTRUCT()
struct FRenderedParagraph
{
    GENERATED_BODY()

    UPROPERTY()
    FText SourceText;

    UPROPERTY()
    float TopY = 0.f;

    UPROPERTY()
    float Height = 0.f;

    UPROPERTY()
    float IndentLeft = 0.f;

    UPROPERTY()
    float IndentRight = 0.f;

    // NEW: the actual wrapped text lines we’ll render
    UPROPERTY()
    TArray<FString> Lines;

    UPROPERTY()
    EGASBlockType BlockType;
};



class ScriptLayoutEngine
{
public:

    /**
     * Computes full screenplay layout: positions, heights, page breaks.
     * Returns array of renderable paragraphs.
     */
    static TArray<FRenderedParagraph> LayoutScript(
        const TArray<FScriptParagraph>& InParagraphs,
        float PanelWidth
    );

private:

    // Formatting helpers
    static float GetLeftIndent(EGASBlockType Type);
    static float GetRightIndent(EGASBlockType Type);


    static float ComputeParagraphHeight(
        const FString& Text,
        float AvailableWidth
    );

    static bool StartsNewBlock(EParagraphType Type);
};
