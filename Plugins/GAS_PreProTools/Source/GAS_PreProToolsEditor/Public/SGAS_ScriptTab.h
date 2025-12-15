#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "ScriptModel.h"
#include "Widgets/SCompoundWidget.h"
#include "GAS_FDXImporter.h"



// Forward declarations
class UGASPreProProject;
class SGASScriptPanel;
class SVerticalBox;
class STextBlock;
class SImage;

struct FShotEntry
{
    int32 StartParagraph;
    int32 EndParagraph;
    FString ShotType;
};


class SGAS_ScriptTab : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGAS_ScriptTab) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void OnScriptParagraphClicked(int32 ParagraphIndex);

    FReply OnImportScript();
    FReply OnSaveScript();


    void ClearScript();

    // =========================================================
    // Project Ownership Handoff
    // =========================================================
    DECLARE_DELEGATE_OneParam(FOnProjectLoaded, UGASPreProProject*);
    FOnProjectLoaded OnProjectLoaded;

    void MarkScriptDirty();
    void ClearScriptDirty();
    bool IsScriptDirty() const;


private:

    // =========================================================
    // User Interactions
    // =========================================================

    // Toggles shot marking mode
    FReply OnToggleShotMarking();

    FReply OnToggleEditMode();


    // =========================================================
    // Rebuild UI Panels
    // =========================================================

    // Right-side shot list
    void RebuildShotList();

    void LoadScriptFromFDX(const FString& FilePath);

    void SaveScriptToJson();
    void LoadScriptFromJsonIfExists();

    float CachedScriptPanelWidth = 1200.f; // fallback width

    FGASScript LoadedScript;
    bool bHasLoadedScript = false;

    FReply OnClearScriptConfirm();
    FReply OnClearScriptClicked();


private:

    // =========================================================
    // Script + Shot Data
    // =========================================================
    TArray<FShotEntry> ShotList;


    bool bIsMarkingShot = false;
    int32 PendingStartParagraph = INDEX_NONE;

    bool bIsShotMarkingActive = false;
    bool bIsEditMode = false;

    bool bIsScriptDirty = false;

    // =========================================================
    // Project / Document
    // =========================================================
    UGASPreProProject* ActiveProject = nullptr;

    // =========================================================
    // Slate UI References
    // =========================================================

    TSharedPtr<SVerticalBox> ShotListContainer;
    TSharedPtr<STextBlock> ShotModeButtonLabel;
    TSharedPtr<SGASScriptPanel> ScriptPanel;
    TSharedPtr<SImage> ShotMarkingIcon;
    TSharedPtr<SImage> NumberingIcon;






};
