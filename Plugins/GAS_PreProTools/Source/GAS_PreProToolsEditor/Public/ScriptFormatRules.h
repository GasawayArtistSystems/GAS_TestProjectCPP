#pragma once
#include "CoreMinimal.h"
#include "GASPreProProject.h"

namespace ScriptFormat
{
    // ------------------------------------------------------------------------
    // Line + Page Metrics
    // ------------------------------------------------------------------------
    static constexpr float LineHeight = 16.f;
    static constexpr float PageWidth = 830.f;
    static constexpr float PageHeight = 920.f;


    static constexpr float MarginTop = 72.f;
    static constexpr float MarginBottom = 72.f;

    static constexpr float LinesPerPageFudge = 1.0f;

    // ------------------------------------------------------------------------
    // Absolute Column Layout (Final Draft–style)
    // ------------------------------------------------------------------------
    static constexpr float PageLeft = 140.f;

    // Relative to printable area (DO NOT include PageLeft)
    static constexpr float SceneCol = 0.f;
    static constexpr float ActionCol = 0.f;
    static constexpr float DialogueCol = 105.f;
    static constexpr float ParentheticalCol = 148.f;
    static constexpr float CharacterCol = 210.f;
    static constexpr float TransitionRightPadding = 42.f;

    // Dialogue width bounds (Final Draft)
    static constexpr float DialogueRight = DialogueCol + 350.f;
    static constexpr float ParentheticalRight = ParentheticalCol + 200.f;


    // Dual dialogue columns (Final Draft)
    static constexpr float DualCharLeftCol = DialogueCol + 30.f;
    static constexpr float DualCharRightCol = DialogueCol + 320.f;

    static constexpr float DualDialogueTextLeftCol = DialogueCol;
    // Extra indent for left-side dual dialogue text (Final Draft nuance)
    static constexpr float DualDialogueTextLeftOffset = 0.f;

    static constexpr float DualDialogueTextRightCol = DialogueCol + 230.f;




    // Dual dialogue
    static constexpr float DualDialogueOffset = 216.f; // ~3"

    inline float GetLeftIndent(EGASBlockType T)
    {
        switch (T)
        {
        case EGASBlockType::SceneHeading:
            return SceneCol;

        case EGASBlockType::Action:
            return ActionCol;

        case EGASBlockType::Character:
            return CharacterCol;

        case EGASBlockType::Dialogue:
            return DialogueCol;

        case EGASBlockType::Parenthetical:
            return ParentheticalCol;

        case EGASBlockType::Transition:
            return 0.f; // handled specially


        default:
            return ActionCol;
        }
    }





    // ------------------------------------------------------------------------
    // LEGACY COMPATIBILITY (DO NOT USE FOR NEW CODE)
    // ------------------------------------------------------------------------

    // These exist ONLY so older layout code compiles.
    // They now map to absolute-column math.

        static constexpr float MarginLeft = PageLeft;
        static constexpr float MarginRight = 72.f; // page right margin (1")

        // Editor column width = printable width
        static constexpr float EditorColumnWidth =
            PageWidth - MarginLeft - MarginRight;


    inline float GetRightIndent(EGASBlockType /*T*/)
    {
        // Legacy API — Final Draft does NOT use right indents.
        // Kept only so ScriptLayoutEngine continues to compile.
        return 0.f;
    }


    inline float GetAvailableWidth(EGASBlockType T)
    {
        switch (T)
        {
        case EGASBlockType::Dialogue:
            return 350.f;

        case EGASBlockType::Parenthetical:
            return 200.f;

        default:
        {
            const float PrintableWidth =
                PageWidth
                - PageLeft
                - MarginRight;

            return PrintableWidth - GetLeftIndent(T);
        }

        }
    }


    // ------------------------------------------------------------------------
    // Uppercasing rules
    // ------------------------------------------------------------------------
    inline bool ShouldUppercase(EGASBlockType T)
    {
        return (
            T == EGASBlockType::SceneHeading ||
            T == EGASBlockType::Character ||
            T == EGASBlockType::Transition ||
            T == EGASBlockType::Shot
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
            return 1.0f;

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
