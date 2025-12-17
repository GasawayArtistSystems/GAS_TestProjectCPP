#pragma once
#include "CoreMinimal.h"
#include "GASPreProProject.h"

namespace ScriptFormat
{
    // ------------------------------------------------------------------------
    // Line + Page Metrics
    // ------------------------------------------------------------------------
    static constexpr float LineHeight = 17.5f;           // Courier 12 FD line height
    static constexpr float EditorColumnWidth = 820.f;   // FD column width
    static constexpr float MarginLeft = 5.f;
    static constexpr float MarginRight = 72.f;
    static constexpr float MarginTop = 72.f;
    static constexpr float MarginBottom = 72.f;

    static constexpr float PageWidth = 612.f;
    static constexpr float PageHeight = 792.f;

    static constexpr float LinesPerPageFudge = 1.0f;

    // ------------------------------------------------------------------------
    // Indentation
    // ------------------------------------------------------------------------
    static constexpr float IndentScene = 144.f;
    static constexpr float IndentAction = 144.f;
    static constexpr float IndentChar = 355.f;
    static constexpr float IndentDialogue = 240.f;
    static constexpr float IndentParen = 298.f;
    static constexpr float IndentTrans = 650.f;

    inline float GetLeftIndent(EGASBlockType T)
    {
        switch (T)
        {
        case EGASBlockType::SceneHeading:
            return IndentScene;

        case EGASBlockType::Action:
            return IndentAction;

        case EGASBlockType::Character:
            return IndentChar;

        case EGASBlockType::Dialogue:
            return IndentDialogue;

        case EGASBlockType::Parenthetical:
            return IndentParen;

        case EGASBlockType::Transition:
            return IndentTrans;

        default:
            return IndentAction;
        }
    }


    inline float GetRightIndent(EGASBlockType T)
    {
        switch (T)
        {
        case EGASBlockType::Dialogue:
        case EGASBlockType::Parenthetical:
        case EGASBlockType::Character:
            return 90.f;   // tighter right margin

        default:
            return 60.f;
        }
    }


    inline float GetAvailableWidth(EGASBlockType T)
    {
        float Base = EditorColumnWidth - GetLeftIndent(T) - GetRightIndent(T);

        return Base;
    }


    // ------------------------------------------------------------------------
    // Uppercasing rules
    // ------------------------------------------------------------------------
    inline bool ShouldUppercase(EGASBlockType T)
    {
        return (
            T == EGASBlockType::SceneHeading ||
            T == EGASBlockType::Character ||
            T == EGASBlockType::Transition
            );
    }

    // ------------------------------------------------------------------------
    // Spacing rules (in LINES, not pixels)
    // ------------------------------------------------------------------------

    static float GetSpacing(EGASBlockType Prev, EGASBlockType Curr)
    {
        //
        // === 0-SPACING GROUPS (FD tight bundles) ===
        //

        // Character → Parenthetical / Dialogue
        if (Prev == EGASBlockType::Character &&
            (Curr == EGASBlockType::Parenthetical || Curr == EGASBlockType::Dialogue))
            return 0.0f;

        // Parenthetical → Dialogue
        if (Prev == EGASBlockType::Parenthetical &&
            Curr == EGASBlockType::Dialogue)
            return 0.0f;

        // Dialogue → Parenthetical
        if (Prev == EGASBlockType::Dialogue &&
            Curr == EGASBlockType::Parenthetical)
            return 0.0f;

        // Dialogue → Dialogue
        if (Prev == EGASBlockType::Dialogue &&
            Curr == EGASBlockType::Dialogue)
            return 0.0f;


        //
        // === Scene Headings ===
        //
        // Anything → SceneHeading gets a bigger gap.
        if (Curr == EGASBlockType::SceneHeading)
        {
            // Action → SceneHeading (FD strong breakup)
            if (Prev == EGASBlockType::Action)
                return 2.0f;

            // Dialogue → SceneHeading
            if (Prev == EGASBlockType::Dialogue)
                return 2.0f;

            // General fallback before headings
            return 1.5f;
        }

        // SceneHeading → Action
        if (Prev == EGASBlockType::SceneHeading &&
            Curr == EGASBlockType::Action)
            return 1.0f;


        //
        // === Action block rules ===
        //

        // Action → Action (FD often ~0.5)
        if (Prev == EGASBlockType::Action &&
            Curr == EGASBlockType::Action)
            return 0.5f;

        // Action → Character (intro a speaker)
        if (Prev == EGASBlockType::Action &&
            Curr == EGASBlockType::Character)
            return 1.0f;

        // Dialogue → Action
        if (Prev == EGASBlockType::Dialogue &&
            Curr == EGASBlockType::Action)
            return 1.0f;


        //
        // === Transitions ===
        //

        if (Curr == EGASBlockType::Transition)
            return 1.0f;

        if (Prev == EGASBlockType::Transition)
            return 1.0f;


        //
        // === DEFAULT SPACING ===
        //

        return 1.0f;   // not 0.0f — FD almost never uses zero for unrelated blocks
    }







}
