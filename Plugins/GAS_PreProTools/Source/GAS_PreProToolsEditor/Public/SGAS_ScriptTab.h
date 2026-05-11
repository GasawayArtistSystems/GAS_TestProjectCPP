#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Math/Color.h"

#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

#include "GAS_FDXImporter.h"
#include "GAS_ShotListTypes.h"
#include "SGASScriptPanel.h"
#include "ScriptModel.h"

// =====================================================
// Forward Declarations
// =====================================================
class UGASPreProProject;
class SGASScriptPanel;
class SGAS_TestWindow;
class SImage;
class STextBlock;
class SVerticalBox;
class SWindow;
class SScrollBox;

struct FGASBlock;
struct FGASImportNumberingOptions;
struct FGASSetDescriptor;

// =====================================================
// Local Helper Structs
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

// =====================================================
// Delegates
// =====================================================
DECLARE_DELEGATE_OneParam(FOnParagraphClicked, int32 /*ParagraphIndex*/);

// =====================================================
// SGAS_ScriptTab
// =====================================================
class SGAS_ScriptTab : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGAS_ScriptTab) {}
    SLATE_END_ARGS()

    // --------------------------------------------------
    // Lifecycle
    // --------------------------------------------------
    void Construct(const FArguments& InArgs);
    ~SGAS_ScriptTab();

    // --------------------------------------------------
    // Delegates
    // --------------------------------------------------
    DECLARE_DELEGATE_OneParam(FOnProjectLoaded, UGASPreProProject*);
    FOnProjectLoaded OnProjectLoaded;

    DECLARE_MULTICAST_DELEGATE(FOnShotListNeedsRefresh);
    FOnShotListNeedsRefresh OnShotListNeedsRefresh;

    DECLARE_DELEGATE(FOnShotModeChanged);
    FOnShotModeChanged OnShotModeChanged;

    // --------------------------------------------------
    // Script / Project API
    // --------------------------------------------------

    void OnScriptParagraphClicked(int32 ParagraphIndex);

    FReply OnImportScript();
    FReply OnSaveScript();
    FReply OnGeneratePageBreaks();
    FReply OnToggleAddMode();

    void ClearScript();
    void EnsureScriptSaved();

    bool CreateNewProject(
        const FString& ProjectName,
        const FGASProjectSettings& InProjectSettings
    );
    bool LoadProject(const FString& AssetPath);
    void PromptCreateNewProject();
    void PromptOpenProject();

    void MarkScriptDirty();
    void ClearScriptDirty();
    bool IsScriptDirty() const;

    FGASScript* GetCurrentScript();
    FGASScript& GetScriptMutable() { return CurrentScript; }

    const FGASScript& GetScript() const
    {
        return CurrentScript;
    }


    // --------------------------------------------------
    // Tool Window / UI State
    // --------------------------------------------------
    void SetMainToolWindow(TSharedPtr<SGAS_TestWindow> InWindow)
    {
        MainToolWindow = InWindow;
    }

    void SetShowShotList(bool bInShow)
    {
        bShowShotList = bInShow;
    }

    bool bShowShotList = false;
    bool bIsSaving = false;



    // --------------------------------------------------
    // Scene Numbering
    // --------------------------------------------------
    void ApplySceneNumberingBaseStyle(EGASSceneNumberBaseStyle InBaseStyle);

    TArray<TSharedPtr<FString>> SceneNumberingStyleOptions;
    TSharedPtr<FString> SceneNumberingStyleSelected;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> SceneNumberingStyleCombo;

    // --------------------------------------------------
    // Shot Mode / Shot Editing
    // --------------------------------------------------
    FSlateColor GetShotButtonTint() const;
    void EnterShotMarkingMode(const FString& SceneBlockId);

    void UpdateShotDescription(const FGuid& ShotId, const FString& NewDescription);
    void UpdateShotNotes(const FGuid& ShotId, const FString& NewNotes);
    void UpdateShotNotesExpanded(const FGuid& ShotId, bool bExpanded);

    void SetActiveBlockingShot(const FGuid& ShotId);
    FGuid GetActiveBlockingShot() const;
    void NotifyShotCameraBound(const FGuid& ShotId);
    void RequestDeleteShotMarker(const FString& MarkerId);

    void OnShotIntentChanged();

    void ClearShotSelectionAfterDelete()
    {
        ActiveShotMarkerId.Empty();
    }

    // --------------------------------------------------
    // Cast / Scene Filtering
    // --------------------------------------------------
    void GetEnabledCastNames(TArray<TSharedPtr<FString>>& OutNames) const;
    bool GetSceneCastForBlockId(const FString& SceneBlockId, TArray<FString>& OutSceneCast) const;
    void GetEnabledCastNames_SceneAware(TArray<TSharedPtr<FString>>& OutNames) const;

    // --------------------------------------------------
    // Blocking / Sets / Sequencer
    // --------------------------------------------------
    void ResumePendingBlocking();
    void LoadSetForBlocking(const FName& SetId);

    void OpenSetSelectionWindow(const FString& SceneId);
    void AssignSetToPendingScene(FName SelectedSetId);
    void SpawnSelectedSet(const FString& SetPath);
    void SpawnSceneCast(FGASBlock* SceneBlock);
    void StopBlocking();

    bool CreateBlockingLevelForScene(
        FGASBlock& SceneBlock,
        const FGASSetDescriptor& SelectedSet
    );

    bool CreateEmptySceneLevel(FGASBlock& SceneBlock);

    void FinalizeBlockingLevel(UWorld* World);

    FGASBlock* GetSceneBlockFromCursor();
    FGASBlock* GetSceneBlockById(const FString& BlockId);

    FString GetBlockingLevelPath(const FString& SceneId) const;
    FString GetSequencePath(const FString& SceneId) const;

    bool IsBlockingLevelOpen(const FString& LevelPath);
    void OpenBlockingLevelIfNeeded(const FString& SceneId);
    void OpenBlockingLevel(const FString& LevelPath);
    void HandleMapOpened(const FString& Filename, bool bAsTemplate);

    // --------------------------------------------------
    // Preview / Camera
    // --------------------------------------------------
    const FSlateBrush* GetPreviewBrush() const
    {
        return PreviewBrush.IsValid() ? PreviewBrush.Get() : nullptr;
    }

    UTextureRenderTarget2D* GetCameraPreviewRenderTarget() const
    {
        return CameraPreviewRenderTarget;
    }

    void InitializeCameraPreview(ACineCameraActor* InSourceCamera);
    void SyncPreviewToRealCamera();
    void ReleaseCameraPreview();
    void UpdateCameraFromShotIntent(FGASShotIntent& Intent);

    void ApplyCameraToLastCreatedShot(
        const FString& MarkerId,
        const FVector& Location,
        const FRotator& Rotation,
        float FocalLength
    );

    TSharedPtr<FGASScript> Script;

    void UpdateShotIntentCameraFromPreview(
        const FString& MarkerId,
        const FVector& Location,
        const FRotator& Rotation,
        float FocalLength
    );

    bool bIsEditingShot = false;
    bool bRestoreSavedCameraThisFrame = false;

    FString PendingShotIntentMarkerId;
    FString PendingShotIntentSceneId;

    // --------------------------------------------------
    // Public Shot-State Flags
    // --------------------------------------------------
    bool bShotAddArmed = false;
    bool bIsShotRangeDragging = false;
    bool bAllowShotHoverPreview = false;
    bool bIsResumingShotMode = false;
    bool bPendingShotResumeAfterMapOpen = false;

    int32 ShotRangeStartParagraph = INDEX_NONE;

    FString ActiveCameraModeSceneId;
    FString ActiveBlockingLevelPath;
    FString PendingShotModeSceneId;
    FString PendingShotModeLevelPath;
    FString PendingResumeSceneId;

    FDelegateHandle OnMapOpenedHandle;

    FText GetNewProjectAspectText() const;

    void SaveScriptToJson();

private:

    // --------------------------------------------------
    // UI / TEMP STATE
    // --------------------------------------------------

    void OnNewProjectAspectChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
    TArray<TSharedPtr<FString>> NewProjectAspectOptions;
    TSharedPtr<FString> NewProjectSelectedAspect;
    TArray<TSharedPtr<FString>> NewProjectFrameRateOptions;
    TSharedPtr<FString> NewProjectSelectedFrameRate;
    TArray<TSharedPtr<FString>> NewProjectSceneNumberingOptions;
    TSharedPtr<FString> NewProjectSelectedSceneNumbering;

    TArray<TSharedPtr<FString>> NewProjectShotNumberingOptions;
    TSharedPtr<FString> NewProjectSelectedShotNumbering;

    TSharedPtr<SSpinBox<int32>> SceneStartNumberSpinBox;
    TSharedPtr<SSpinBox<int32>> ShotStartNumberSpinBox;
    FGASProjectSettings PendingProjectSettings;

    void OnNewProjectFrameRateChanged(
        TSharedPtr<FString> NewValue,
        ESelectInfo::Type SelectInfo
    );

    FText GetNewProjectFrameRateText() const;

    void OnNewProjectSceneNumberingChanged(
        TSharedPtr<FString> NewValue,
        ESelectInfo::Type SelectInfo
    );

    FText GetNewProjectSceneNumberingText() const;

    void OnNewProjectShotNumberingChanged(
        TSharedPtr<FString> NewValue,
        ESelectInfo::Type SelectInfo
    );

    FText GetNewProjectShotNumberingText() const;
    void OnSceneStartNumberChanged(int32 NewValue);
    void OnShotStartNumberChanged(int32 NewValue);
    void OnEnableBlockingChanged(ECheckBoxState NewState);
    void OnEnableShotLayersChanged(ECheckBoxState NewState);
    void OnAutoMasterSequenceChanged(ECheckBoxState NewState);

    // --------------------------------------------------
    // UI Commands / Interactions
    // --------------------------------------------------
    FReply OnToggleEditMode();
    FReply OnToggleSceneNumbers();
    FReply OnAddShotMarkerClicked();
    FReply OnOpenCastPopup();

    FReply OnClearScriptConfirm();
    FReply OnClearScriptClicked();

    // --------------------------------------------------
    // Internal Script / Persistence Helpers
    // --------------------------------------------------
    void RebuildShotList();
    void BuildCastListFromScript();
    void RebuildCastUI();
    void RebuildCastList();

    void LoadScriptFromFDX(
        const FString& FilePath,
        const FGASImportNumberingOptions& ImportOptions
    );

    FString GetAuthoritativeScriptJsonPath() const;
    void LoadScriptFromJsonIfExists();
    

    bool TickShotModeResume(float DeltaTime);

    // --------------------------------------------------
    // Blocking UI / Scene Actions
    // --------------------------------------------------
    void ScrollToScene(const FGASShotDefinitionListRow& Scene);
    void ScrollToSceneBlock(const FString& BlockId);

    void OnStartBlockingScene(const FString& SceneId);
    void OnDeleteBlockingScene(const FString& SceneId);
    void OpenCastWindowForScene(const FString& SceneId);
    void OnBlockingCastModified();
    bool bPendingAutoOpenCastWindow = false;
    bool bIsUpdatingCast = false;

    void DeleteShotMarkerNow(const FString& MarkerId);

    bool bPendingDeleteShotAfterMapOpen = false;
    FString PendingDeleteShotMarkerId;
    FString PendingDeleteShotLevelPath;

    TSharedPtr<SWindow> PendingActionWindow;

    void ShowPendingActionWindow(const FString& Message);
    void UpdatePendingActionWindow(const FString& Message);
    void ClosePendingActionWindow();


#if WITH_EDITOR
    void BindToExistingShotCameras();
    void HandleShotCameraMoved(const FString& MarkerId, const FTransform& NewTransform);
    void RehydrateShotCamerasForScene(const FString& SceneBlockId);

    TSet<TWeakObjectPtr<class AGAS_ShotCameraActor>> BoundShotCameras;
#endif

    // --------------------------------------------------
    // Preview Helpers
    // --------------------------------------------------
    void UpdateShotIntentPreview();
    void ConfigurePreviewCameraFilmback(UCineCameraComponent* InCamera) const;
    FVector2D GetPreviewWidgetSize(float InWidth) const;

    // --------------------------------------------------
    // Authoritative Model
    // --------------------------------------------------
    FGASScript CurrentScript;

    // --------------------------------------------------
    // Project / Dirty State
    // --------------------------------------------------
    UPROPERTY()
    UGASPreProProject* ActiveProject = nullptr;

    UGASPreProProject* GetActiveProject() const
    {
        return ActiveProject;
    }

    bool bIsScriptDirty = false;

    // --------------------------------------------------
    // Editor / Mode State
    // --------------------------------------------------
    bool bIsEditMode = false;
    bool bIsAddMode = false;
    bool bShowSceneNumbers = false;

    FGuid ActiveBlockingShotId;
    FString ActiveShotMarkerId;

    // --------------------------------------------------
    // Cast State
    // --------------------------------------------------
    TArray<FGASCastMember> CastList;
    TSharedPtr<SVerticalBox> CastListContainer;

    // --------------------------------------------------
    // Blocking State
    // --------------------------------------------------
    FString PendingBlockingSceneId;
    FGuid ActiveBlockingSceneId;
    bool bBlockingActive = false;

    TSharedPtr<SWindow> ActiveSetBrowserWindow;
    TWeakPtr<SWindow> BlockingCastWindow;

    // --------------------------------------------------
    // Main UI References
    // --------------------------------------------------
    TWeakPtr<SGAS_TestWindow> MainToolWindow;

    TSharedPtr<SVerticalBox> ShotListContainer;
    TSharedPtr<STextBlock> ShotModeButtonLabel;
    TSharedPtr<SGASScriptPanel> ScriptPanel;
    TSharedPtr<SImage> ShotMarkingIcon;
    TSharedPtr<SImage> NumberingIcon;
    TSharedPtr<SScrollBox> ScriptScrollBox;

    // --------------------------------------------------
    // Layout / Shot List UI
    // --------------------------------------------------
    float CachedScriptPanelWidth = 1200.f;

    TArray<TSharedPtr<FGASShotDefinitionListRow>> ShotListItems;
    TSet<FString> ExpandedScenes;

    float ColExpand = 0.10f;
    float ColPage = 0.08f;
    float ColScene = 0.15f;
    float ColHeading = 1.0f;
    float ColLength = 0.20f;
    float ColTime = 0.50f;
    float ColSet = 0.50f;
    float ColNotes = 1.0f;

    float ColShot = 0.10f;
    float ColType = 0.10f;
    float ColDes = 0.60f;
    float ColLens = 0.10f;
    float ColCamera = 0.20f;

    // --------------------------------------------------
    // Shot Intent Preview State
    // --------------------------------------------------
    TSharedPtr<FString> SelectedShotIntentSubject;
    TSharedPtr<FString> SelectedShotIntentType;

    UWorld* PreviewWorld = nullptr;

    UPROPERTY()
    ACineCameraActor* ShotCamera = nullptr;

    TWeakObjectPtr<ACineCameraActor> PreviewSourceCamera;

    UPROPERTY(Transient)
    USceneCaptureComponent2D* PreviewCaptureComponent = nullptr;

    TSharedPtr<FSlateBrush> PreviewBrush;
    TSharedPtr<FSlateBrush> CameraPreviewBrush;
    TSharedPtr<SImage> ShotPreviewImage;

    // --------------------------------------------------
    // Camera Preview Render System
    // --------------------------------------------------
    static constexpr int32 PreviewRTWidth = 1280;
    static constexpr int32 PreviewRTHeight = 720;
    static constexpr float PreviewAspect = 16.0f / 9.0f;

    UPROPERTY(Transient)
    UTextureRenderTarget2D* CameraPreviewRenderTarget = nullptr;

    UPROPERTY()
    TObjectPtr<ACineCameraActor> PreviewCameraActor = nullptr;

    UPROPERTY()
    TObjectPtr<UCineCameraComponent> PreviewCameraComponent = nullptr;

    UPROPERTY()
    TObjectPtr<USceneCaptureComponent2D> PreviewSceneCapture = nullptr;

    UPROPERTY()
    UTextureRenderTarget2D* ShotPreviewRenderTarget = nullptr;
};