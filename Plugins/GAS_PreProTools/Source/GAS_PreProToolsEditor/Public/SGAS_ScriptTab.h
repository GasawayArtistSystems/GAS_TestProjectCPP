#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "ScriptModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Styling/SlateColor.h"
#include "Math/Color.h"

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

// =====================================================
// Cast Management (Editor-only)
// =====================================================
struct FGASCastMember
{
    FString Name;
    bool bEnabled = true;
};


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


    // ------------------------------------------------------------
    // Shot Marker (Phase 1)
    // ------------------------------------------------------------
    bool bShotSelectArmed = false;
    FSlateColor GetShotButtonTint() const;

    
    void AddShotMarkerForScene(const FString& SceneBlockId);

    void GetEnabledCastNames(TArray<TSharedPtr<FString>>& OutNames) const;

    void UpdateShotDescription(const FGuid& ShotId, const FString& NewDescription);




private:

    // --------------------------------------------------
    // Cast (derived from script, user-editable later)
    // --------------------------------------------------
    TArray<FGASCastMember> CastList;
    void BuildCastListFromScript();
    TSharedPtr<SVerticalBox> CastListContainer;
    void RebuildCastUI();
    FReply OnOpenCastPopup();
    void RebuildCastList();


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


    // -----------------------------------------
    // Shot List (Scene rows from script JSON)
    // -----------------------------------------
    TArray<TSharedPtr<FGASShotDefinitionListRow>> ShotListItems;
    void ScrollToScene(const FGASShotDefinitionListRow& Scene);
    TSharedPtr<SScrollBox> ScriptScrollBox;

    void ScrollToSceneBlock(const FString& BlockId);

    // -------------------------------------------------
    // Shot List UI state
    // -------------------------------------------------
    TSet<FString> ExpandedScenes;

    float ColExpand = 0.10f;
    float ColPage = 0.08f;
    float ColScene = 0.15f;
    float ColHeading = 1.0f;
    float ColLength = 0.20f;
    float ColTime = 0.20f;
    float ColSet = 0.50f;
    float ColNotes = 1.0f;

    float ColShot = 0.10f;
    float ColType = 0.10f;

    float ColDes = 0.60f;
    float ColLens = 0.10f;
    float ColCamera = 0.20f;

    // --------------------------------------------------
    // Shot selection
    // --------------------------------------------------
    FString ActiveShotMarkerId;


};
