#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "GAS_FDXImporter.h"


// Forward declarations
class SGASScriptPanel;
class SVerticalBox;
class STextBlock;
class SImage;

struct FShotEntry
{
    int32 StartParagraph = 0;
    int32 EndParagraph = 0;
    FString ShotType = TEXT("Unknown");
};

class SGAS_ScriptTab : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGAS_ScriptTab) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void OnScriptParagraphClicked(int32 ParagraphIndex);

    FReply OnImportScript();



private:

    // =========================================================
    // User Interactions
    // =========================================================

    // Toggles shot marking mode
    FReply OnToggleShotMarking();

    // Remove old numbering toggle
    // FReply OnToggleNumbering();  <-- DELETE THIS LINE

    // =========================================================
    // Rebuild UI Panels
    // =========================================================

    // Right-side shot list
    void RebuildShotList();

    void LoadScriptFromFDX(const FString& FilePath);

    void SaveScriptToJson();
    void LoadScriptFromJsonIfExists();

    float CachedScriptPanelWidth = 1200.f; // fallback width


private:

    // =========================================================
    // Script + Shot Data
    // =========================================================

    // Parsed screenplay paragraphs (raw from importer)
    TArray<FScriptParagraph> ParsedParagraphs;

    // Paragraph-based shot markers
    TArray<int32> ShotStartParagraphs;
    TArray<int32> ShotEndParagraphs;

    // Shot metadata
    TArray<FShotEntry> ShotList;

    bool bIsMarkingShot = false;
    int32 PendingStartParagraph = INDEX_NONE;

    bool bIsShotMarkingActive = false;


    // Remove numbering flags
    // bool bShowNumbering = false;
    // bool bShowSceneAndDialogueNumbers = false;

    // =========================================================
    // Slate UI References
    // =========================================================

    TSharedPtr<SVerticalBox> ShotListContainer;
    TSharedPtr<STextBlock> ShotModeButtonLabel;
    TSharedPtr<SGASScriptPanel> ScriptPanel;
    TSharedPtr<SImage> ShotMarkingIcon;
    TSharedPtr<SImage> NumberingIcon;
};
