#pragma once

#include "CoreMinimal.h"

// Forward declaration ONLY.
// No Editor-module includes allowed in Public headers.
struct FRenderedParagraph;

// ============================================================
// GAS Shot List – Page/Eighths Math (DECLARATIONS)
// ============================================================
//
// Rules (LOCKED):
// - Page height is defined by user-placed page breaks
// - A page is divided into 8 equal vertical slices
// - Length is stored as TOTAL EIGHTHS
// - ALWAYS round UP
//
// Implementation lives in .cpp (Editor-safe)
//
// ============================================================

namespace GASShotListEighths
{
    struct FPageSpan
    {
        int32 PageNumber = 1;   // 1-based
        float StartY = 0.f;     // Inclusive
        float EndY = 0.f;     // Exclusive
    };

    void BuildPageSpans(
        const TArray<FRenderedParagraph>& Paragraphs,
        TArray<FPageSpan>& OutPages
    );

    int32 ComputeEighthsBetween_Up(
        const TArray<FPageSpan>& Pages,
        int32 StartPage,
        float StartY,
        int32 EndPage,
        float EndY
    );

    int32 ComputeEighthsFromLayout_Up(
        const TArray<FRenderedParagraph>& Paragraphs,
        int32 StartPage,
        float StartY,
        int32 EndPage,
        float EndY
    );

    // Shot-only eighths (ignores page breaks)
    int32 ComputeShotEighths_Up(
        float StartY,
        float EndY,
        float EighthHeightY
    );

}

FString GAS_FormatPagesEighths(int32 TotalEighths);
