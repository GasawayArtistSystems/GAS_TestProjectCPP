#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "ScriptModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

#include "GAS_FDXImporter.h"
#include "SGASScriptPanel.h"
#include "GAS_ShotListTypes.h"


// Forward declarations
class UGASPreProProject;
class SGASScriptPanel;
class SVerticalBox;
class STextBlock;
class SImage;
class SGAS_TestWindow;

struct FShotEntry
{
    int32 StartParagraph;
    int32 EndParagraph;
    FString ShotType;
};

DECLARE_DELEGATE_OneParam(FOnParagraphClicked, int32 /*ParagraphIndex*/);

class SGAS_ScriptTab : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SGAS_ScriptTab) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void OnScriptParagraphClicked(int32 ParagraphIndex);

    FReply OnImportScript();
    FReply OnSaveScript();

    FReply OnGeneratePageBreaks();
    FReply OnToggleAddMode();

    void ClearScript();

    // =========================================================
    // Project Ownership Handoff
    // =========================================================
    DECLARE_DELEGATE_OneParam(FOnProjectLoaded, UGASPreProProject*);
    FOnProjectLoaded OnProjectLoaded;

    void MarkScriptDirty();
    void ClearScriptDirty();
    bool IsScriptDirty() const;
    bool bShowShotList = false;


    void SetMainToolWindow(TSharedPtr<SGAS_TestWindow> InWindow)
    {
        MainToolWindow = InWindow;
    }

    // If script is dirty, silently auto-save JSON (no prompts, no popups).
    // Safe to call repeatedly.
    void EnsureScriptSaved();


    void SetShowShotList(bool bInShow)
    {
        bShowShotList = bInShow;

    }

    // --------------------------------------------------
    // Scene Numbering UI
    // --------------------------------------------------
    TArray<TSharedPtr<FString>> SceneNumberingStyleOptions;
    TSharedPtr<FString> SceneNumberingStyleSelected;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> SceneNumberingStyleCombo;

    // Apply scene numbering policy and rebuild views
    void ApplySceneNumberingBaseStyle(EGASSceneNumberBaseStyle InBaseStyle);


private:

    // =========================================================
    // User Interactions
    // =========================================================

    FReply OnToggleShotMarking();
    FReply OnToggleEditMode();
    FReply OnToggleSceneNumbers();
    FReply OnAddShotMarkerClicked();

    // =========================================================
    // Rebuild UI Panels
    // =========================================================

    void RebuildShotList();
    void LoadScriptFromFDX(
        const FString& FilePath,
        const FGASImportNumberingOptions& ImportOptions
    );



    FString GetAuthoritativeScriptJsonPath() const;
    void LoadScriptFromJsonIfExists();
    void SaveScriptToJson();

    float CachedScriptPanelWidth = 1200.f;

    // =========================================================
    // Authoritative Script Model (JSON-backed)
    // =========================================================
    FGASScript CurrentScript;
    bool bHasScript = false;

    FReply OnClearScriptConfirm();
    FReply OnClearScriptClicked();

    TWeakPtr<SGAS_TestWindow> MainToolWindow;

private:

    // =========================================================
    // Script + Shot Data
    // =========================================================
    TArray<FShotEntry> ShotList;

    bool bIsShotMarkingActive = false;
    int32 PendingStartParagraph = INDEX_NONE;
    bool bIsEditMode = false;

    // ★ EXISTING — this is the authoritative dirty flag
    bool bIsScriptDirty = false;

    bool bIsAddMode = false;
    bool bShowSceneNumbers = false;

    // =========================================================
    // Project / Document
    // =========================================================
    UPROPERTY()
    UGASPreProProject* ActiveProject = nullptr;

    // =========================================================
    // Slate UI References
    // =========================================================
    TSharedPtr<SVerticalBox> ShotListContainer;
    TSharedPtr<STextBlock> ShotModeButtonLabel;
    TSharedPtr<SGASScriptPanel> ScriptPanel;
    TSharedPtr<SImage> ShotMarkingIcon;
    TSharedPtr<SImage> NumberingIcon;

    bool SaveScriptJson_Silent();

    // -----------------------------------------
    // Shot List (Scene rows from script JSON)
    // -----------------------------------------
    TArray<TSharedPtr<FGASShotDefinitionListRow>> ShotListItems;
    void ScrollToScene(const FGASShotDefinitionListRow& Scene);
    TSharedPtr<SScrollBox> ScriptScrollBox;

    void ScrollToSceneBlock(const FString& BlockId);


};
