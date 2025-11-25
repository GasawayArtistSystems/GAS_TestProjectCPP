#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// Forward declarations
class SGASScriptPanel;
class SVerticalBox;
class STextBlock;
class SImage;

struct FShotEntry
{
    int32 StartLineIndex = 0;
    int32 EndLineIndex = 0;
    float ShotX = 0.f;
    FString ShotType = TEXT("Unknown");   // later: “Medium”, “Closeup”, etc.
};


/**
 * Main Script Tab for GAS Pre Pro Tools
 * Handles shot-marking and script display.
 */
class SGAS_ScriptTab : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGAS_ScriptTab) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // User clicks a script line (start/end shot)
    void OnScriptLineClicked(int32 LineIndex, float ClickX);

private:

    // =========================================================
    // User Interactions
    // =========================================================

    // Toggles shot marking mode
    FReply OnToggleShotMarking();

    // Toggles scene/dialog numbering
    FReply OnToggleNumbering();

    // =========================================================
    // Rebuild UI Panels
    // =========================================================

    // Rebuilds the right-side shot list panel
    void RebuildShotList();

    // 🔹 Rebuild script panel (used after numbering changes)
    void RebuildScriptList();

    // Compute scene & dialogue numbering arrays
    void ComputeLineNumbers(
        TArray<FString>& OutSceneNumbers,
        TArray<FString>& OutDialogueNumbers
    );

private:

    // =========================================================
    // Script + Shot Data
    // =========================================================

    TArray<FString> ScriptLines;         // Full screenplay text
    TArray<FShotEntry> ShotList;         // Shot ranges (start/end line numbers + X position)

    bool bIsMarkingShot = false;         // Whether user is picking start→end
    int32 PendingStartLineIndex = INDEX_NONE;
    float PendingStartX = 0.f;           // X for the first click of a shot

    bool bShowNumbering = false;
    bool bShowSceneAndDialogueNumbers = false;

    // =========================================================
    // Scene & Dialogue Numbering Data
    // =========================================================

    TArray<FString> SceneNums;           // One array entry per script line
    TArray<FString> DialogueNums;        // One array entry per script line

    // =========================================================
    // Slate UI References
    // =========================================================

    TSharedPtr<SVerticalBox> ShotListContainer; // Right-side shot list
    TSharedPtr<STextBlock>   ShotModeButtonLabel; // Begin / Cancel
    TSharedPtr<SGASScriptPanel> ScriptPanel;      // Middle panel that draws lines
    TSharedPtr<SImage> ShotMarkingIcon;           // Camera icon image
    TSharedPtr<SImage> NumberingIcon;

};
