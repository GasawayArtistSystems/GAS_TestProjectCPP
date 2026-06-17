#include "SGAS_ScriptTab.h"
#include "GAS_StageLightingUtils.h"
#include "GAS_ShotListBuilder.h"
#include "ScriptModel.h"
#include "SGASScriptPanel.h"
#include "GAS_FDXImporter.h"
#include "GASPreProProject.h"
#include "GAS_PreProToolsEditorModule.h"
#include "GAS_ImportNumberingTypes.h"
#include "GAS_ShotListTypes.h"
#include "GAS_ShotIntentTypes.h"
#include "GAS_SetManager.h"
#include "SGASShotCastList.h"
#include "ShotList/GAS_ShotListEighths.h"
#include "SScriptWheelCatcher.h"
#include "ShotList/GAS_ShotListBuilderV2.h"
#include "GAS_PreProToolsStyle.h"
#include "GASScriptAsset.h"
#include "GAS_SetActor.h"
#include "SGAS_SetBrowser.h"
#include "GAS_SetDiscovery.h"
#include "Actors/GAS_StageCenterActor.h"
#include "GAS_StandInActor.h"
#include "UI/Blocking/SGAS_BlockingCastWindow.h"
#include "GAS_BlockingAnchorActor.h"
#include "Actors/GAS_ShotCameraActor.h"
#include "GASPreProLog.h"
#include "GASBlockingAccess.h"
#include "GAS_SequencerBindingUtils.h"
#include "GAS_SceneNumberResolver.h"

#include "ScriptLayoutEngine.h"
#include "JsonObjectConverter.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SThrobber.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "FileHelpers.h"
#include "Misc/MessageDialog.h"
#include "EditorLevelUtils.h"
#include "Editor.h"
#include "LevelEditorSubsystem.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ObjectTools.h"
#include "IAssetTools.h"
#include "PackageTools.h"
#include "Framework/Docking/TabManager.h"
#include "EngineUtils.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/SlateBrush.h"

#include "HAL/FileManager.h"
#include "Factories/WorldFactory.h"
#include "EditorSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "LevelSequence.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "ILevelSequenceEditorToolkit.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"

#include "MovieScene.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneCameraCutSection.h"

ACineCameraActor* GLastCreatedCamera = nullptr;

// TEMP!!!
void SGAS_ScriptTab::UpdateShotIntentPreview()
{
}

// =======================================================
// Fixed panel widths (UI constants)
// =======================================================
static constexpr float ScriptPanelWidth = 810.f;
static constexpr float ShotListPanelWidth = 980.f;

// =======================================================
// Camera ratio
// =======================================================
static FVector2D GAS_ParseAspectRatio(const FString& InRatio)
{
    FString Left, Right;

    // Case 1: "16:9"
    if (InRatio.Split(TEXT(":"), &Left, &Right))
    {
        return FVector2D(FCString::Atof(*Left), FCString::Atof(*Right));
    }

    // Case 2: "1920x1080"
    if (InRatio.Split(TEXT("x"), &Left, &Right))
    {
        return FVector2D(FCString::Atof(*Left), FCString::Atof(*Right));
    }

    // Fallback
    return FVector2D(16.f, 9.f);
}

static FIntPoint GAS_GetRenderTargetSize(const FString& Aspect)
{
    const FVector2D Ratio = GAS_ParseAspectRatio(Aspect);

    const float BaseHeight = 720.f;
    const float Width = BaseHeight * (Ratio.X / Ratio.Y);

    return FIntPoint((int32)Width, (int32)BaseHeight);
}

FText SGAS_ScriptTab::GetNewProjectAspectText() const
{
    return NewProjectSelectedAspect.IsValid()
        ? FText::FromString(*NewProjectSelectedAspect)
        : FText::FromString(TEXT("Select Aspect Ratio"));
}

FText SGAS_ScriptTab::GetNewProjectFrameRateText() const
{
    if (NewProjectSelectedFrameRate.IsValid())
    {
        return FText::FromString(*NewProjectSelectedFrameRate);
    }

    return FText::FromString(TEXT("24"));

}

FText SGAS_ScriptTab::GetNewProjectSceneNumberingText() const
{
    if (NewProjectSelectedSceneNumbering.IsValid())
    {
        return FText::FromString(
            *NewProjectSelectedSceneNumbering
        );
    }

    return FText::FromString(TEXT("By 10"));
}

FText SGAS_ScriptTab::GetNewProjectShotNumberingText() const
{
    if (NewProjectSelectedShotNumbering.IsValid())
    {
        return FText::FromString(
            *NewProjectSelectedShotNumbering
        );
    }

    return FText::FromString(TEXT("By 1"));
}

void SGAS_ScriptTab::OnNewProjectAspectChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type)
{
    if (NewValue.IsValid())
    {
        NewProjectSelectedAspect = NewValue;

        PendingProjectSettings.AspectRatio = *NewValue;
    }
}

void SGAS_ScriptTab::OnNewProjectFrameRateChanged(
    TSharedPtr<FString> NewValue,
    ESelectInfo::Type)
{
    if (!NewValue.IsValid())
    {
        return;
    }

    NewProjectSelectedFrameRate = NewValue;

    if (*NewValue == TEXT("23.976"))
    {
        PendingProjectSettings.FrameRate =
            EGASProjectFrameRate::FPS_23_976;
    }
    else if (*NewValue == TEXT("24"))
    {
        PendingProjectSettings.FrameRate =
            EGASProjectFrameRate::FPS_24;
    }
    else if (*NewValue == TEXT("25"))
    {
        PendingProjectSettings.FrameRate =
            EGASProjectFrameRate::FPS_25;
    }
    else if (*NewValue == TEXT("29.97"))
    {
        PendingProjectSettings.FrameRate =
            EGASProjectFrameRate::FPS_29_97;
    }
    else if (*NewValue == TEXT("30"))
    {
        PendingProjectSettings.FrameRate =
            EGASProjectFrameRate::FPS_30;
    }
}


void SGAS_ScriptTab::OnNewProjectSceneNumberingChanged(
    TSharedPtr<FString> NewValue,
    ESelectInfo::Type)
{
    if (!NewValue.IsValid())
    {
        return;
    }

    NewProjectSelectedSceneNumbering = NewValue;

    if (*NewValue == TEXT("By 10"))
    {
        PendingProjectSettings.SceneNumberingStyle =
            EGASNumberingStyle::CountBy10;

        PendingProjectSettings.SceneStartNumber = 10;

        if (SceneStartNumberSpinBox.IsValid())
        {
            SceneStartNumberSpinBox->SetValue(10);
            SceneStartNumberSpinBox->SetEnabled(true);
        }
    }
    else if (*NewValue == TEXT("By 1"))
    {
        PendingProjectSettings.SceneNumberingStyle =
            EGASNumberingStyle::CountBy1;

        PendingProjectSettings.SceneStartNumber = 1;

        if (SceneStartNumberSpinBox.IsValid())
        {
            SceneStartNumberSpinBox->SetValue(1);
            SceneStartNumberSpinBox->SetEnabled(true);
        }
    }
    else if (*NewValue == TEXT("Alphabetic"))
    {
        PendingProjectSettings.SceneNumberingStyle =
            EGASNumberingStyle::Alphabetic;

        PendingProjectSettings.SceneStartNumber = 1;

        if (SceneStartNumberSpinBox.IsValid())
        {
            SceneStartNumberSpinBox->SetValue(1);
            SceneStartNumberSpinBox->SetEnabled(false);
        }
    }
    else if (*NewValue == TEXT("Use FDX"))
    {
        PendingProjectSettings.SceneNumberingStyle =
            EGASNumberingStyle::FromScript;

        if (SceneStartNumberSpinBox.IsValid())
        {
            SceneStartNumberSpinBox->SetEnabled(false);
        }
    }
}

void SGAS_ScriptTab::OnNewProjectShotNumberingChanged(
    TSharedPtr<FString> NewValue,
    ESelectInfo::Type)
{
    if (!NewValue.IsValid())
    {
        return;
    }

    NewProjectSelectedShotNumbering = NewValue;

    if (*NewValue == TEXT("By 1"))
    {
        PendingProjectSettings.ShotNumberingPolicy =
            EGASShotNumberingPolicy::Numeric_1s;

        PendingProjectSettings.ShotStartNumber = 1;

        if (ShotStartNumberSpinBox.IsValid())
        {
            ShotStartNumberSpinBox->SetValue(1);
            ShotStartNumberSpinBox->SetEnabled(true);
        }
    }
    else if (*NewValue == TEXT("By 10"))
    {
        PendingProjectSettings.ShotNumberingPolicy =
            EGASShotNumberingPolicy::Numeric_10s;

        PendingProjectSettings.ShotStartNumber = 10;

        if (ShotStartNumberSpinBox.IsValid())
        {
            ShotStartNumberSpinBox->SetValue(10);
            ShotStartNumberSpinBox->SetEnabled(true);
        }
    }
    else if (*NewValue == TEXT("Alphabetic"))
    {
        PendingProjectSettings.ShotNumberingPolicy =
            EGASShotNumberingPolicy::Alphabetic;

        PendingProjectSettings.ShotStartNumber = 1;

        if (ShotStartNumberSpinBox.IsValid())
        {
            ShotStartNumberSpinBox->SetValue(1);
            ShotStartNumberSpinBox->SetEnabled(false);
        }
    }
}

void SGAS_ScriptTab::OnSceneStartNumberChanged(int32 NewValue)
{
    PendingProjectSettings.SceneStartNumber = NewValue;
}

void SGAS_ScriptTab::OnShotStartNumberChanged(int32 NewValue)
{
    PendingProjectSettings.ShotStartNumber = NewValue;
}


// =======================================================
// FOR BLOCKING SETUP
// =======================================================

bool SGAS_ScriptTab::SceneHasNonBlockingShots(
    const FString& SceneId
) const
{
    for (const FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        if (Marker.BlockId != SceneId)
        {
            continue;
        }

        if (Marker.IsNonBlocking())
        {
            return true;
        }
    }

    return false;
}

bool SGAS_ScriptTab::SceneHasBlockingShots(
    const FString& SceneId
) const
{
    for (const FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        if (Marker.BlockId != SceneId)
        {
            continue;
        }

        if (Marker.IsBlocking())
        {
            return true;
        }
    }

    return false;
}

void SGAS_ScriptTab::PromoteNonBlockingShotsToBlocking(
    const FString& SceneId
)
{
    int32 PromoteCount = 0;

    for (FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        if (Marker.BlockId != SceneId)
        {
            continue;
        }

        if (!Marker.IsNonBlocking())
        {
            continue;
        }

        Marker.ShotOrigin = EGASShotOrigin::Blocking;

        Marker.SetSpatialState(
            EGASShotSpatialState::BlockingReady
        );

        PromoteCount++;
    }

    if (PromoteCount > 0)
    {
        UE_LOG(
            LogGASPrePro,
            Warning,
            TEXT("Promoted %d NonBlocking shots for scene %s"),
            PromoteCount,
            *SceneId
        );

        MarkScriptDirty();
        EnsureScriptSaved();
    }
}

static void SaveSequenceAsset(ULevelSequence* Sequence)
{
    if (!IsValid(Sequence))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("SaveSequenceAsset: Sequence NULL or invalid"));
        return;
    }

    UObject* OuterObject = Sequence->GetOuter();
    if (!IsValid(OuterObject))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("SaveSequenceAsset: Outer invalid"));
        return;
    }

    UPackage* Package = Sequence->GetOutermost();
    if (!IsValid(Package))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("SaveSequenceAsset: Package NULL or invalid"));
        return;
    }

    const FString PackageName = Package->GetName();

    if (PackageName.IsEmpty() ||
        PackageName == TEXT("None") ||
        !FPackageName::IsValidLongPackageName(PackageName, false))
    {
        UE_LOG(
            LogGASPrePro,
            Error,
            TEXT("SaveSequenceAsset: Invalid PackageName [%s]"),
            *PackageName
        );
        return;
    }

    const FString Filename =
        FPackageName::LongPackageNameToFilename(
            PackageName,
            FPackageName::GetAssetPackageExtension()
        );

    if (Filename.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Error, TEXT("SaveSequenceAsset: Filename empty for package %s"), *PackageName);
        return;
    }

    Package->MarkPackageDirty();

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSaved = UPackage::SavePackage(
        Package,
        Sequence,
        *Filename,
        SaveArgs
    );

    UE_LOG(
        LogGASPrePro,
        Warning,
        TEXT("SaveSequenceAsset: %s -> %s"),
        *PackageName,
        bSaved ? TEXT("SUCCESS") : TEXT("FAILED")
    );
}



static ULevelSequence* GActiveBlockingSequence = nullptr;

ULevelSequence* SGAS_ScriptTab::GetOrCreateSceneSequence(
    const FGASBlock& SceneBlock
)
{
    if (!ActiveProject)
    {
        return nullptr;
    }

    // Use scene number as the authoritative name — not heading text
    const TMap<FString, FString> SceneNumberMap =
        FGASSceneNumberResolver::ResolveSceneNumbers(
            CurrentScript.Blocks,
            CurrentScript.SceneNumbering
        );

    FString SceneNumber = SceneNumberMap.FindRef(SceneBlock.Id);

    if (SceneNumber.IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("GetOrCreateSceneSequence: No scene number for block %s"),
            *SceneBlock.Id);
        SceneNumber = SceneBlock.Id.Left(8);
    }

    SceneNumber.ReplaceInline(TEXT(" "), TEXT("_"));

    const FString ProjectName = ActiveProject->ProjectName;

    const FString AssetName =
        FString::Printf(TEXT("SC_%s"), *SceneNumber);

    const FString PackagePath =
        FString::Printf(
            TEXT("/Game/PrePro/Projects/%s/Sequences/Scenes/%s"),
            *ProjectName,
            *AssetName
        );


    // ------------------------------------------------------------
    // LOAD EXISTING
    // ------------------------------------------------------------

    ULevelSequence* Existing =
        LoadObject<ULevelSequence>(
            nullptr,
            *PackagePath
        );

    if (Existing)
    {
        return Existing;
    }

    // ------------------------------------------------------------
    // CREATE NEW
    // ------------------------------------------------------------

    UPackage* Package =
        CreatePackage(*PackagePath);

    if (!Package)
    {
        return nullptr;
    }

    ULevelSequence* NewSequence =
        NewObject<ULevelSequence>(
            Package,
            *AssetName,
            RF_Public | RF_Standalone
        );

    if (!NewSequence)
    {
        return nullptr;
    }

    NewSequence->Initialize();

    UMovieScene* MovieScene =
        NewSequence->GetMovieScene();

    if (MovieScene)
    {
        FFrameRate SequenceRate(24, 1);

        if (ActiveProject)
        {
            switch (ActiveProject->ProjectSettings.FrameRate)
            {
            case EGASProjectFrameRate::FPS_23_976:
                SequenceRate = FFrameRate(24000, 1001);
                break;

            case EGASProjectFrameRate::FPS_24:
                SequenceRate = FFrameRate(24, 1);
                break;

            case EGASProjectFrameRate::FPS_25:
                SequenceRate = FFrameRate(25, 1);
                break;

            case EGASProjectFrameRate::FPS_29_97:
                SequenceRate = FFrameRate(30000, 1001);
                break;

            case EGASProjectFrameRate::FPS_30:
                SequenceRate = FFrameRate(30, 1);
                break;
            }

            MovieScene->SetDisplayRate(SequenceRate);
            MovieScene->SetTickResolutionDirectly(SequenceRate);
        }
    }

    FAssetRegistryModule::AssetCreated(NewSequence);

    SaveSequenceAsset(NewSequence);

    UE_LOG(LogTemp, Warning,
        TEXT("Created Scene Sequence: %s"),
        *AssetName);

    return NewSequence;
}

void SGAS_ScriptTab::CreateOrUpdateMasterSequence()
{
    if (!ActiveProject)
    {
        UE_LOG(LogTemp, Error,
            TEXT("CreateOrUpdateMasterSequence: No ActiveProject"));
        return;
    }

    const FString ProjectName =
        ActiveProject->ProjectName;

    const FString MasterPackagePath =
        FString::Printf(
            TEXT("/Game/PrePro/Projects/%s/Sequences/MASTER_%s"),
            *ProjectName,
            *ProjectName
        );

    ULevelSequence* MasterSequence =
        LoadObject<ULevelSequence>(
            nullptr,
            *MasterPackagePath
        );

    // ------------------------------------------------------------
    // CREATE MASTER IF MISSING
    // ------------------------------------------------------------

    if (!MasterSequence)
    {
        FString AssetName;
        FString PackagePath;

        MasterPackagePath.Split(
            TEXT("/"),
            &PackagePath,
            &AssetName,
            ESearchCase::IgnoreCase,
            ESearchDir::FromEnd
        );

        UPackage* Package =
            CreatePackage(*MasterPackagePath);

        MasterSequence =
            NewObject<ULevelSequence>(
                Package,
                *AssetName,
                RF_Public | RF_Standalone
            );

        MasterSequence->Initialize();

        FAssetRegistryModule::AssetCreated(
            MasterSequence
        );

        UE_LOG(LogTemp, Warning,
            TEXT("Created MASTER sequence"));
    }

    if (!MasterSequence)
    {
        return;
    }

    UMovieScene* MovieScene =
        MasterSequence->GetMovieScene();

    FFrameRate SequenceRate(24, 1);

    if (ActiveProject)
    {
        switch (ActiveProject->ProjectSettings.FrameRate)
        {
        case EGASProjectFrameRate::FPS_23_976:
            SequenceRate = FFrameRate(24000, 1001);
            break;

        case EGASProjectFrameRate::FPS_24:
            SequenceRate = FFrameRate(24, 1);
            break;

        case EGASProjectFrameRate::FPS_25:
            SequenceRate = FFrameRate(25, 1);
            break;

        case EGASProjectFrameRate::FPS_29_97:
            SequenceRate = FFrameRate(30000, 1001);
            break;

        case EGASProjectFrameRate::FPS_30:
            SequenceRate = FFrameRate(30, 1);
            break;
        }
    }

    MovieScene->SetDisplayRate(SequenceRate);
    MovieScene->SetTickResolutionDirectly(SequenceRate);

    if (!MovieScene)
    {
        return;
    }

    // ------------------------------------------------------------
    // FIND / CREATE SUBTRACK
    // ------------------------------------------------------------

    UMovieSceneSubTrack* SubTrack = nullptr;

    for (UMovieSceneTrack* Track : MovieScene->GetTracks())
    {
        SubTrack = Cast<UMovieSceneSubTrack>(Track);

        if (SubTrack)
        {
            break;
        }
    }

    if (!SubTrack)
    {
        SubTrack =
            NewObject<UMovieSceneSubTrack>(
                MovieScene,
                NAME_None,
                RF_Transactional
            );

        MovieScene->AddGivenTrack(SubTrack);
    }

    if (!SubTrack)
    {
        return;
    }

    // ------------------------------------------------------------
    // CLEAR EXISTING
    // ------------------------------------------------------------

    SubTrack->Modify();
    MovieScene->Modify();
    MasterSequence->Modify();

    SubTrack->RemoveAllAnimationData();

    // ------------------------------------------------------------
    // BUILD SCENE ROWS (authoritative eighths from layout)
    // ------------------------------------------------------------

    constexpr float SecondsPerEighth = 7.5f;

    TArray<FGASShotListSceneRow> SceneRows;

    BuildShotListFromScriptV2(
        CurrentScript,
        CurrentScript.SceneNumbering,
        ScriptPanel->GetRenderedParagraphs(),
        SceneRows
    );

    // ------------------------------------------------------------
    // ADD SCENE SUBSEQUENCES
    // ------------------------------------------------------------

    int32 CurrentFrame = 0;

    const FFrameRate TickResolution =
        MovieScene->GetTickResolution();

    const float FPS =
        TickResolution.AsDecimal();

    for (const FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Type != EGASBlockType::SceneHeading)
        {
            continue;
        }

        ULevelSequence* SceneSequence =
            GetOrCreateSceneSequence(Block);

        if (!SceneSequence)
        {
            continue;
        }

        // Look up real eighths for this scene
        float SceneEighths = 4.0f; // fallback: half a page

        const FGASShotListSceneRow* FoundScene = SceneRows.FindByPredicate(
            [&Block](const FGASShotListSceneRow& Row)
            {
                return Row.SceneId == Block.Id;
            }
        );

        if (FoundScene && FoundScene->LengthEighths > 0)
        {
            SceneEighths = (float)FoundScene->LengthEighths;
        }

        const int32 DurationFrames =
            FMath::RoundToInt(
                (SceneEighths * SecondsPerEighth) * FPS
            );

        UMovieSceneSubSection* SubSection =
            SubTrack->AddSequence(
                SceneSequence,
                CurrentFrame,
                DurationFrames
            );

        if (!SubSection)
        {
            continue;
        }

        UE_LOG(LogTemp, Warning,
            TEXT("MASTER ADD SECTION: %s"),
            *SceneSequence->GetName());

        UE_LOG(LogTemp, Warning,
            TEXT("SECTION RANGE: %d -> %d  (%.1f eighths)"),
            CurrentFrame,
            CurrentFrame + DurationFrames,
            SceneEighths
        );



        CurrentFrame += DurationFrames + 1;
    }

    // ------------------------------------------------------------
    // FINALIZE
    // ------------------------------------------------------------

    MovieScene->SetPlaybackRange(
        0,
        CurrentFrame
    );

    MovieScene->SetWorkingRange(
        0.0,
        (double)CurrentFrame / FPS
    );

    MovieScene->SetViewRange(
        0.0,
        (double)CurrentFrame / FPS
    );

    MovieScene->MarkAsChanged();

    FPropertyChangedEvent DummyEvent(nullptr);

    MovieScene->PostEditChangeProperty(
        DummyEvent
    );

    MasterSequence->MarkPackageDirty();

    UE_LOG(LogTemp, Warning,
        TEXT("TOTAL MASTER FRAMES: %d"),
        CurrentFrame
    );

    SaveSequenceAsset(MasterSequence);

    UE_LOG(LogTemp, Warning,
        TEXT("MASTER sequence updated"));
}


void SGAS_ScriptTab::AddCameraTracksToSceneSequence(
    ULevelSequence* SceneSequence,
    const FGASBlock& SceneBlock,
    const TArray<FGASShotListSceneRow>& SceneRows
)
{
    if (!SceneSequence || !ActiveProject)
        return;

    UMovieScene* MovieScene = SceneSequence->GetMovieScene();
    if (!MovieScene)
        return;

    // Skip if blocking level isn't currently loaded
    if (!GEditor)
        return;

    UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
    if (!CurrentWorld)
        return;

    const FString CurrentMapName = CurrentWorld->GetMapName();
    const FString ExpectedMapName = FPackageName::GetShortName(SceneBlock.BlockingLevelPath);

    if (!ExpectedMapName.IsEmpty() && !CurrentMapName.Contains(ExpectedMapName))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[CameraCuts] Skipping scene %s — blocking level not loaded"),
            *SceneBlock.Text);
        return;
    }

    const float FPS = MovieScene->GetTickResolution().AsDecimal();
    constexpr float SecondsPerEighth = 7.5f;

    // ------------------------------------------------------------
    // Find this scene's total frame count
    // ------------------------------------------------------------
    float SceneEighths = 4.f;

    const FGASShotListSceneRow* FoundScene =
        SceneRows.FindByPredicate(
            [&](const FGASShotListSceneRow& Row)
            {
                return Row.SceneId == SceneBlock.Id;
            }
        );

    if (FoundScene && FoundScene->LengthEighths > 0)
        SceneEighths = (float)FoundScene->LengthEighths;

    const int32 TotalSceneFrames =
        FMath::RoundToInt(SceneEighths * SecondsPerEighth * FPS);

    // ------------------------------------------------------------
    // Find / create camera cut track
    // ------------------------------------------------------------
    UMovieSceneCameraCutTrack* CutTrack = nullptr;

    for (UMovieSceneTrack* Track : MovieScene->GetTracks())
    {
        CutTrack = Cast<UMovieSceneCameraCutTrack>(Track);
        if (CutTrack)
            break;
    }

    if (!CutTrack)
    {
        CutTrack = Cast<UMovieSceneCameraCutTrack>(
            MovieScene->AddTrack(
                UMovieSceneCameraCutTrack::StaticClass()
            )
        );
    }

    if (!CutTrack)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[CameraCuts] Failed to create CameraCutTrack for %s"),
            *SceneBlock.Text);
        return;
    }

    CutTrack->Modify();
    CutTrack->RemoveAllAnimationData();

    // ------------------------------------------------------------
    // Collect shots for this scene
    // ------------------------------------------------------------
    TArray<const FGASMarker*> SceneShots;

    for (const FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType == EGASMarkerType::ShotMarker &&
            Marker.BlockId == SceneBlock.Id)
        {
            SceneShots.Add(&Marker);
        }
    }

    // Sort by script position
    SceneShots.Sort(
        [](const FGASMarker& A, const FGASMarker& B)
        {
            return A.ShotLineStartY < B.ShotLineStartY;
        }
    );

    if (SceneShots.Num() == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[CameraCuts] No shots for scene: %s"),
            *SceneBlock.Text);
        return;
    }

    // Get scene Y range from rendered paragraphs
    float SceneStartY = 0.f;
    float SceneEndY = 0.f;

    if (ScriptPanel.IsValid())
    {
        const TArray<FRenderedParagraph>& Rendered =
            ScriptPanel->GetRenderedParagraphs();

        // Find scene heading Y
        for (const FRenderedParagraph& P : Rendered)
        {
            if (P.BlockId == SceneBlock.Id)
            {
                SceneStartY = P.TopY;
                break;
            }
        }

        // Find next scene heading Y (or end of script)
        bool bFoundScene = false;
        for (const FRenderedParagraph& P : Rendered)
        {
            if (P.BlockId == SceneBlock.Id)
            {
                bFoundScene = true;
                continue;
            }

            if (bFoundScene &&
                P.BlockType == EGASBlockType::SceneHeading)
            {
                SceneEndY = P.TopY;
                break;
            }
        }

        // Fallback: use last paragraph
        if (SceneEndY <= SceneStartY && Rendered.Num() > 0)
        {
            const FRenderedParagraph& Last = Rendered.Last();
            SceneEndY = Last.TopY + Last.Height;
        }
    }

    const float SceneYSpan = FMath::Max(1.f, SceneEndY - SceneStartY);

    // ------------------------------------------------------------
    // Add camera cut section for each shot
    // ------------------------------------------------------------
    for (const FGASMarker* Shot : SceneShots)
    {
        const FGASShotIntent* Intent =
            CurrentScript.ShotIntents.Find(Shot->Id);

        if (!Intent || !IsValid(Intent->CameraActor))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[CameraCuts] No camera for shot %s — skipping"),
                *Shot->Id);
            continue;
        }

        // Calculate frames directly from eighths
        const int32 ShotEighths =
            GASShotListEighths::ComputeShotEighths_Up(
                Shot->ShotLineStartY,
                Shot->ShotLineEndY,
                ScriptFormat::EighthHeight
            );

        const float ShotSeconds =
            FMath::Max(1, ShotEighths) * SecondsPerEighth;

        // Start frame from script position ratio
        const float NormStart =
            (Shot->ShotLineStartY - SceneStartY) / SceneYSpan;

        const int32 StartFrame =
            FMath::RoundToInt(NormStart * TotalSceneFrames);

        const int32 DurationFrames =
            FMath::Max(1, FMath::RoundToInt(ShotSeconds * FPS));

        const int32 EndFrame = StartFrame + DurationFrames;

        // Resolve soft pointer first
        AActor* CamActor = Intent->CameraActor.Get();
        if (!IsValid(CamActor))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[CameraCuts] No camera actor for shot %s — skipping"),
                *Shot->Id);
            continue;
        }

        // Check if already bound
        FGuid CameraGuid;
        for (int32 PossIdx = 0; PossIdx < MovieScene->GetPossessableCount(); ++PossIdx)
        {
            const FMovieScenePossessable& Existing =
                MovieScene->GetPossessable(PossIdx);
            if (Existing.GetName() == CamActor->GetName())
            {
                CameraGuid = Existing.GetGuid();
                break;
            }
        }
        if (!CameraGuid.IsValid())
        {
            if (!IsValid(CamActor) || !CamActor->GetWorld())
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[CameraCuts] Camera world invalid, skipping"));
                continue;
            }

            CameraGuid = MovieScene->AddPossessable(
                CamActor->GetName(),
                CamActor->GetClass()
            );

            SceneSequence->BindPossessableObject(
                CameraGuid,
                *CamActor,
                CamActor->GetWorld()
            );
        }
        if (!CameraGuid.IsValid())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[CameraCuts] Could not bind camera for shot %s"),
                *Shot->Id);
            continue;
        }

        // Add cut section
        UMovieSceneCameraCutSection* CutSection =
            Cast<UMovieSceneCameraCutSection>(
                CutTrack->CreateNewSection()
            );

        if (CutSection)
        {
            CutTrack->AddSection(*CutSection);
        }

        if (!CutSection)
            continue;

        CutSection->SetRange(
            TRange<FFrameNumber>(
                FFrameNumber(StartFrame),
                FFrameNumber(EndFrame)
            )
        );

        CutSection->SetCameraGuid(CameraGuid);

        UE_LOG(LogTemp, Warning,
            TEXT("[CameraCuts] Added cut: Shot=%s Camera=%s Frames=%d-%d"),
            *Shot->Id,
            *CamActor->GetName(),
            StartFrame,
            EndFrame
        );
    }

    MovieScene->MarkAsChanged();
    FPropertyChangedEvent DummyEvent(nullptr);
    MovieScene->PostEditChangeProperty(DummyEvent);
    SceneSequence->MarkPackageDirty();

    SaveSequenceAsset(SceneSequence);
}


static bool DuplicateSetIntoBlockingLevel(
    const FString& SetLevelPath,
    UWorld* BlockingWorld
)
{
    if (!BlockingWorld)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("DuplicateSetIntoBlockingLevel: BlockingWorld NULL"));
        return false;
    }

    // -----------------------------------------------------
    // Prevent duplicate set bake
    // -----------------------------------------------------
    for (AActor* ExistingActor : BlockingWorld->PersistentLevel->Actors)
    {
        if (!ExistingActor)
            continue;

        FString Label = ExistingActor->GetActorLabel();

        if (Label.EndsWith(TEXT("_SetRoot")))
        {
            UE_LOG(LogGASPrePro, Warning,
                TEXT("DuplicateSetIntoBlockingLevel: Set already baked. Skipping."));
            return true;
        }
    }

    // -----------------------------------------------------
    // Load set world as asset (DO NOT LoadMap)
    // -----------------------------------------------------
    UPackage* SetPackage = LoadPackage(nullptr, *SetLevelPath, LOAD_None);
    if (!SetPackage)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("DuplicateSetIntoBlockingLevel: Failed to load package %s"), *SetLevelPath);
        return false;
    }

    UWorld* SetWorld = UWorld::FindWorldInPackage(SetPackage);
    if (!SetWorld)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("DuplicateSetIntoBlockingLevel: No world in package"));
        return false;
    }

    ULevel* SetLevel = SetWorld->PersistentLevel;
    ULevel* BlockingLevel = BlockingWorld->PersistentLevel;

    if (!SetLevel || !BlockingLevel)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("DuplicateSetIntoBlockingLevel: Invalid levels"));
        return false;
    }

    // -----------------------------------------------------
    // First pass: spawn actors and store mapping
    // -----------------------------------------------------
    TMap<AActor*, AActor*> ActorMap;

    for (AActor* Actor : SetLevel->Actors)
    {
        if (!Actor)
            continue;

        if (Actor->IsEditorOnly())
            continue;

        if (Actor->IsTemplate())
            continue;

        if (Actor->GetClass()->HasAnyClassFlags(CLASS_Transient))
            continue;

        FActorSpawnParameters SpawnParams;
        SpawnParams.OverrideLevel = BlockingLevel;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AActor* NewActor = BlockingWorld->SpawnActor<AActor>(
            Actor->GetClass(),
            Actor->GetActorTransform(),
            SpawnParams
        );

        if (NewActor)
        {
            EditorUtilities::CopyActorProperties(Actor, NewActor);

            // 🔥 Force proper editor registration
            NewActor->ReregisterAllComponents();
            NewActor->MarkComponentsRenderStateDirty();

            ActorMap.Add(Actor, NewActor);
        }
    }

    // -----------------------------------------------------
    // Second pass: restore attachments
    // -----------------------------------------------------
    for (const TPair<AActor*, AActor*>& Pair : ActorMap)
    {
        AActor* OldActor = Pair.Key;
        AActor* NewActor = Pair.Value;

        AActor* OldParent = OldActor->GetAttachParentActor();
        if (OldParent && ActorMap.Contains(OldParent))
        {
            AActor* NewParent = ActorMap[OldParent];
            NewActor->AttachToActor(
                NewParent,
                FAttachmentTransformRules::KeepRelativeTransform
            );
        }
    }

    // -----------------------------------------------------
    // Cleanup set package (prevent hidden world leaks)
    // -----------------------------------------------------
    SetWorld->MarkAsGarbage();
    SetPackage->MarkAsGarbage();

    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

    return true;
}

static void GAS_SplitHeading_TimeOfDay(FString& InOutHeading, FString& OutTimeOfDay)
{
    OutTimeOfDay.Reset();

    FString H = InOutHeading.TrimStartAndEnd();

    int32 DashIdx = H.Find(TEXT(" - "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);

    if (DashIdx == INDEX_NONE)
    {
        DashIdx = H.Find(TEXT(" — "), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    }

    if (DashIdx == INDEX_NONE)
    {
        InOutHeading = H;
        return;
    }

    FString Left = H.Left(DashIdx).TrimStartAndEnd();
    FString Right = H.Mid(DashIdx + 3).TrimStartAndEnd();

    if (Right.IsEmpty())
    {
        InOutHeading = H;
        return;
    }

    while (Right.EndsWith(TEXT(".")) ||
        Right.EndsWith(TEXT(",")) ||
        Right.EndsWith(TEXT(";")) ||
        Right.EndsWith(TEXT(":")))
    {
        Right.LeftChopInline(1);
        Right = Right.TrimStartAndEnd();
    }

    if (Right.IsEmpty())
    {
        InOutHeading = H;
        return;
    }

    InOutHeading = Left;
    OutTimeOfDay = Right;
}


// ------------------------------------------------------------
// Import-time numbering prompt (Scene + Shot)
// ------------------------------------------------------------
static FGASImportNumberingOptions PromptImportNumberingOptions()
{
    FGASImportNumberingOptions Options;

    // Defaults (match previous behavior)
    Options.SceneNumberingStyle = EGASNumberingStyle::CountBy10;
    Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Numeric_1s;

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("Scene & Shot Numbering")))
        .ClientSize(FVector2D(420.f, 320.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    // ------------------------------------------------------------
    // Scene numbering checkboxes
    // ------------------------------------------------------------
    TSharedPtr<SCheckBox> SceneBy10;
    TSharedPtr<SCheckBox> SceneBy1;
    TSharedPtr<SCheckBox> SceneAlpha;
    TSharedPtr<SCheckBox> RespectScriptSceneNumbers;


    // ------------------------------------------------------------
    // Shot numbering checkboxes
    // ------------------------------------------------------------
    TSharedPtr<SCheckBox> ShotBy1;
    TSharedPtr<SCheckBox> ShotBy10;
    TSharedPtr<SCheckBox> ShotAlpha;

    Window->SetContent(
        SNew(SVerticalBox)

        // =========================================================
        // Header
        // =========================================================
        +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(
                    TEXT("Choose how scenes and shots should be numbered.\n")
                    TEXT("(These cannot be changed later.)")
                ))
        ]

    // =========================================================
    // Scene numbering source
    // =========================================================
    +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f, 8.f, 12.f, 4.f)
        [
            SAssignNew(RespectScriptSceneNumbers, SCheckBox)
                .IsChecked(ECheckBoxState::Unchecked) // default: OVERRIDE script numbers
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        Options.bRespectScriptSceneNumbers =
                            (State == ECheckBoxState::Checked);

                        if (State == ECheckBoxState::Checked)
                        {
                            Options.SceneNumberingStyle = EGASNumberingStyle::FromScript;

                            SceneBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                        }
                    })

                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Use scene numbers already in the FDX (if present)")))
                ]
        ]


    // =========================================================
    // Scene Numbering
    // =========================================================
    +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f, 8.f, 12.f, 4.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Scene Numbering Style")))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(SceneBy10, SCheckBox)
                .IsChecked(ECheckBoxState::Checked)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            SceneBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.SceneNumberingStyle = EGASNumberingStyle::CountBy10;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("By 10 (10, 20, 30…)")))
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(SceneBy1, SCheckBox)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            SceneBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.SceneNumberingStyle = EGASNumberingStyle::CountBy1;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("By 1 (1, 2, 3…)")))
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(SceneAlpha, SCheckBox)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            SceneBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            SceneBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.SceneNumberingStyle = EGASNumberingStyle::Alphabetic;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("Alphabetical (A, B, C…)")))
                ]
        ]

    // =========================================================
    // Shot Numbering
    // =========================================================
    +SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f, 10.f, 12.f, 4.f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Shot Numbering Style")))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(ShotBy1, SCheckBox)
                .IsChecked(ECheckBoxState::Checked)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            ShotBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            ShotAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Numeric_1s;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("By 1 (1, 2, 3…)")))
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(ShotBy10, SCheckBox)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            ShotBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            ShotAlpha->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Numeric_1s;
                        }
                    })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("By 10 (10, 20, 30…)")))
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(20.f, 2.f)
        [
            SAssignNew(ShotAlpha, SCheckBox)
                .OnCheckStateChanged_Lambda([&](ECheckBoxState State)
                    {
                        if (State == ECheckBoxState::Checked)
                        {
                            ShotBy1->SetIsChecked(ECheckBoxState::Unchecked);
                            ShotBy10->SetIsChecked(ECheckBoxState::Unchecked);
                            Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Alphabetic;
                        }
                    })
                [
                    SNew(STextBlock).Text(
                        FText::FromString(TEXT("Alphabetical (A, B, C… AA after Z)"))
                    )
                ]
        ]

    // =========================================================
    // OK button
    // =========================================================
    +SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(FMargin(12.f, 12.f, 12.f, 24.f)) // extra bottom padding
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("OK")))
                .OnClicked_Lambda([&]()
                    {
                        Window->RequestDestroyWindow();
                        return FReply::Handled();
                    })
        ]

        );

    FSlateApplication::Get().AddModalWindow(Window, nullptr);

    return Options;
}

void SGAS_ScriptTab::RebuildCastList()
{
    if (!CastListContainer.IsValid())
    {
        return;
    }

    CastListContainer->ClearChildren();

    TArray<TSharedPtr<FString>> EnabledCast;
    GetEnabledCastNames(EnabledCast);

    for (FGASCastMember& Member : CastList)
    {
        CastListContainer->AddSlot()
            .AutoHeight()
            .Padding(2.f)
            [
                SNew(SCheckBox)
                    .IsChecked_Lambda([&Member]()
                        {
                            return Member.bEnabled
                                ? ECheckBoxState::Checked
                                : ECheckBoxState::Unchecked;
                        })
                    .OnCheckStateChanged_Lambda([this, &Member](ECheckBoxState NewState)
                        {
                            Member.bEnabled = (NewState == ECheckBoxState::Checked);

                            TArray<TSharedPtr<FString>> EnabledNames;
                            GetEnabledCastNames(EnabledNames);

                            if (ScriptPanel.IsValid())
                            {
                                ScriptPanel->SetEnabledCastNames(EnabledNames);
                            }
                        })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Member.Name.ToUpper()))
                    ]
            ];
    }

}

FReply SGAS_ScriptTab::OnOpenCastPopup()
{

    // Ensure script is fully synced before opening Cast
    CurrentScript.PostScriptMutation();

    TSharedRef<SWindow> CastWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Cast")))
        .ClientSize(FVector2D(320.f, 420.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    CastWindow->SetContent(
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .FillHeight(1.f)
        [
            SNew(SGASShotCastList)
                .Script(&CurrentScript)

        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(8.f, 12.f)
        [
            SNew(SButton)
                .Text(FText::FromString(TEXT("Close")))
                .OnClicked_Lambda(
                    [CastWindow]()
                    {
                        CastWindow->RequestDestroyWindow();
                        return FReply::Handled();
                    }
                )
        ]
    );


    FSlateApplication::Get().AddWindow(CastWindow);

    return FReply::Handled();
}

static void GAS_ForceSequencerEval(UWorld* World)
{
    if (!World) return;

    for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
    {
        ALevelSequenceActor* SeqActor = *It;

        if (SeqActor && SeqActor->GetSequencePlayer())
        {
            ULevelSequencePlayer* Player = SeqActor->GetSequencePlayer();

            FMovieSceneSequencePlaybackParams Params;
            Params.PositionType = EMovieScenePositionType::Frame;
            Params.Frame = Player->GetCurrentTime().Time.FrameNumber;

            Player->SetPlaybackPosition(Params);

            UE_LOG(LogTemp, Warning, TEXT("PRE-SAVE SEQUENCER EVAL"));
        }
    }
}


void SGAS_ScriptTab::ShowPendingActionWindow(const FString& Message)
{
    if (PendingActionWindow.IsValid())
    {
        UpdatePendingActionWindow(Message);
        return;
    }

    TSharedRef<STextBlock> MessageText =
        SNew(STextBlock)
        .Text(FText::FromString(Message));

    PendingActionWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Please Wait")))
        .ClientSize(FVector2D(280.f, 90.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false)
        .HasCloseButton(false)
        .SizingRule(ESizingRule::FixedSize)
        .IsTopmostWindow(true);

    PendingActionWindow->SetContent(
        SNew(SBorder)
        .Padding(12.f)
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.f, 0.f, 10.f, 0.f)
                [
                    SNew(SThrobber)
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.f)
                .VAlign(VAlign_Center)
                [
                    MessageText
                ]
        ]
    );

    PendingActionWindow->SetTag(FName(*Message));

    FSlateApplication::Get().AddWindow(PendingActionWindow.ToSharedRef());
}

void SGAS_ScriptTab::UpdatePendingActionWindow(const FString& Message)
{
    if (!PendingActionWindow.IsValid())
    {
        ShowPendingActionWindow(Message);
        return;
    }

    TSharedRef<STextBlock> MessageText =
        SNew(STextBlock)
        .Text(FText::FromString(Message));

    PendingActionWindow->SetContent(
        SNew(SBorder)
        .Padding(12.f)
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.f, 0.f, 10.f, 0.f)
                [
                    SNew(SThrobber)
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.f)
                .VAlign(VAlign_Center)
                [
                    MessageText
                ]
        ]
    );
}

void SGAS_ScriptTab::ClosePendingActionWindow()
{
    if (PendingActionWindow.IsValid())
    {
        PendingActionWindow->RequestDestroyWindow();
        PendingActionWindow.Reset();
    }
}

void SGAS_ScriptTab::RequestDeleteShotMarker(const FString& MarkerId)
{
    if (CurrentScript.Blocks.Num() == 0)
    {
        return;
    }

    // ------------------------------------------------------------
    // Find the shot marker
    // ------------------------------------------------------------
    const FGASMarker* Marker = CurrentScript.Markers.FindByPredicate(
        [&](const FGASMarker& M)
        {
            return M.Id == MarkerId;
        }
    );

    if (!Marker)
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("[DeleteShot] Marker not found: %s"), *MarkerId);
        return;
    }

    const FString SceneId = Marker->BlockId;

    // ------------------------------------------------------------
    // Resolve blocking level
    // ------------------------------------------------------------
    const FString BlockingPath = GetBlockingLevelPath(SceneId);

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[DeleteShot] Scene=%s BlockingPath=%s"),
        *SceneId,
        *BlockingPath
    );

    // ------------------------------------------------------------
    // If correct level is already open → delete immediately
    // ------------------------------------------------------------
    if (IsBlockingLevelOpen(BlockingPath))
    {
        DeleteShotMarkerNow(MarkerId);
        return;
    }

    // ------------------------------------------------------------
    // Otherwise → queue delete and open level
    // ------------------------------------------------------------
    bPendingDeleteShotAfterMapOpen = true;
    PendingDeleteShotMarkerId = MarkerId;
    PendingDeleteShotLevelPath = BlockingPath;

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[DeleteShot] Opening level before delete: %s"),
        *BlockingPath
    );

    ShowPendingActionWindow(TEXT("Opening blocking scene..."));

    OpenBlockingLevel(BlockingPath);
}

void SGAS_ScriptTab::DeleteShotMarkerNow(const FString& MarkerId)
{
    ResetShotModeState();

    FScopedSlowTask SlowTask(
        0.f,
        FText::FromString(TEXT("Deleting Shot"))
    );

    SlowTask.MakeDialog(false);

    if (MarkerId.IsEmpty())
    {
        return;
    }

    // ------------------------------------------------------------
    // Find camera info before removing shot intent
    // ------------------------------------------------------------
    TWeakObjectPtr<AGAS_ShotCameraActor> CameraPtr = nullptr;

    if (FGASShotIntent* Intent = CurrentScript.ShotIntents.Find(MarkerId))
    {
        CameraPtr = Intent->CameraActor;
    }

#if WITH_EDITOR
    if (GEditor)
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            // ----------------------------------------------------
            // First try the stored camera pointer
            // ----------------------------------------------------
            if (CameraPtr.IsValid())
            {
                if (AGAS_ShotCameraActor* Camera = CameraPtr.Get())
                {
                    UE_LOG(LogGASPrePro, Warning,
                        TEXT("[DeleteShot] Destroying camera actor: %s"),
                        *Camera->GetName());

                    Camera->Destroy();
                }
            }
            else
            {
                UE_LOG(LogGASPrePro, Warning,
                    TEXT("[DeleteShot] No camera actor found for marker: %s"),
                    *MarkerId);
            }
        }
    }
#endif

    // ------------------------------------------------------------
    // Remove marker
    // ------------------------------------------------------------
    CurrentScript.Markers.RemoveAll(
        [&](const FGASMarker& M)
        {
            return M.Id == MarkerId;
        }
    );

    // ------------------------------------------------------------
    // Remove shot intent
    // ------------------------------------------------------------
    CurrentScript.ShotIntents.Remove(MarkerId);

    ActiveShotMarkerId.Empty();

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[DeleteShot][BEFORE REBUILD] MarkerCount=%d IntentCount=%d"),
        CurrentScript.Markers.Num(),
        CurrentScript.ShotIntents.Num()
    );

    UE_LOG(LogTemp, Warning, TEXT("=== FINAL MARKERS BEFORE UI ==="));

    for (const FGASMarker& M : CurrentScript.Markers)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("MarkerId=%s ShotIndex=%d"),
            *M.Id,
            M.ShotIndex
        );
    }


    MarkScriptDirty();
    SaveScriptToJson();
    RebuildShotList();

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[DeleteShot][AFTER REBUILD] MarkerCount=%d IntentCount=%d"),
        CurrentScript.Markers.Num(),
        CurrentScript.ShotIntents.Num()
    );

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->RebuildLayout();
        ScriptPanel->Invalidate(EInvalidateWidget::Paint);
    }

}


void SGAS_ScriptTab::ApplyCameraToLastCreatedShot(
    const FString& MarkerId,
    const FVector& Location,
    const FRotator& Rotation,
    float FocalLength
)
{
    UE_LOG(LogTemp, Error,
        TEXT("[CAM SAVE] CurrentScript Ptr=%p MarkerCount=%d ShotIntentCount=%d"),
        &CurrentScript,
        CurrentScript.Markers.Num(),
        CurrentScript.ShotIntents.Num());

    UE_LOG(LogTemp, Error,
        TEXT("[CAM SAVE] ApplyCameraToLastCreatedShot CALLED MarkerId=%s Loc=%s"),
        *MarkerId,
        *Location.ToString());

    FGASShotIntent* Shot = CurrentScript.ShotIntents.Find(MarkerId);

    if (!Shot)
    {
        return;
    }

    // ------------------------------------------------------------
    // SAVE DATA
    // ------------------------------------------------------------
    Shot->CameraLocation = Location;
    Shot->CameraRotation = Rotation;
    Shot->CameraFocalLength = FocalLength;

    // ------------------------------------------------------------
    // AUTHORITATIVE MARKER CAMERA PERSISTENCE
    // ------------------------------------------------------------
    for (FGASMarker& Marker : CurrentScript.Markers)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[CAM SAVE] Checking Marker=%s Against=%s"),
            *Marker.Id,
            *MarkerId);

        if (Marker.Id == MarkerId)
        {
            const FTransform CameraTransform(Rotation, Location);

            Marker.BindCameraTransform(CameraTransform);

            UE_LOG(LogTemp, Error,
                TEXT("[CAM SAVE] Marker persisted Id=%s bHasCamera=%d Transform=%s"),
                *Marker.Id,
                Marker.bHasCamera ? 1 : 0,
                *Marker.CameraTransform.ToString());

            UE_LOG(LogTemp, Warning,
                TEXT("AUTHORITATIVE CAMERA SAVED: %s"),
                *MarkerId);

            MarkScriptDirty();
            EnsureScriptSaved();

            break;
        }
    }

    UE_LOG(LogTemp, Error,
        TEXT("[CAM SAVE] Finished marker search"));

    UE_LOG(LogTemp, Warning, TEXT("Camera Saved: %s"), *Location.ToString());

    // ------------------------------------------------------------
    // APPLY TO REAL CAMERA (CRITICAL FIX)
    // ------------------------------------------------------------
    if (IsValid(Shot->CameraActor))
    {
        AGAS_ShotCameraActor* Cam = Shot->CameraActor;

        if (Cam)
        {
            Cam->SetActorLocationAndRotation(Location, Rotation);

            if (Cam->CineCamera)
            {
                Cam->CineCamera->SetCurrentFocalLength(FocalLength);
            }
        }
    }
}


void SGAS_ScriptTab::UpdateCameraFromShotIntent(FGASShotIntent& Intent)
{
    // REAL CAMERA ONLY.
    // Do not call this for preview-only shot intent editing.
    // This function may spawn/bind a real AGAS_ShotCameraActor.

    const FGASMarker* Marker =
        CurrentScript.Markers.FindByPredicate(
            [&](const FGASMarker& M)
            {
                return M.Id == Intent.MarkerId;
            }
        );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[BLOCKING CHECK] Intent.MarkerId=%s MarkerFound=%s"),
        *Intent.MarkerId,
        Marker ? TEXT("YES") : TEXT("NO")
    );

    if (!Marker || !Marker->IsBlocking())
    {
        return;
    }

    if (!GEditor) return;

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return;

    // --------------------------------------------------
    // Resolve camera
    // --------------------------------------------------
    AGAS_ShotCameraActor* Cam = Intent.CameraActor;

    // Fallback: search level for existing camera with matching MarkerId
    if (!IsValid(Cam))
    {
        for (TActorIterator<AGAS_ShotCameraActor> It(World); It; ++It)
        {
            if (It->MarkerId == Intent.MarkerId)
            {
                Cam = *It;
                Intent.CameraActor = Cam;
                Intent.BoundCameraMarkerId = Intent.MarkerId;
                break;
            }
        }
    }

    if (!IsValid(Cam))
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        FString CameraLabel = TEXT("ShotCam");

        if (Marker)
        {
            CameraLabel =
                FString::Printf(
                    TEXT("SHOT_%s"),
                    *Marker->GetShotLabel(
                        CurrentScript.ProjectSettings.ShotNumberingPolicy
                    )
                );
        }

        ULevel* TargetLevel = World->PersistentLevel;

        SpawnParams.OverrideLevel = TargetLevel;

        SpawnParams.Name = MakeUniqueObjectName(
            TargetLevel,
            AGAS_ShotCameraActor::StaticClass(),
            FName(*CameraLabel)
        );

        UE_LOG(LogTemp, Warning,
            TEXT("CREATE CAMERA | Marker=%s | CameraLabel=%s"),
            *Intent.MarkerId,
            *CameraLabel
        );

        Cam = World->SpawnActor<AGAS_ShotCameraActor>(
            AGAS_ShotCameraActor::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParams
        );

        if (!IsValid(Cam))
        {
            UE_LOG(LogTemp, Error,
                TEXT("[GAS CameraPreview] Failed to create blocking camera for MarkerId=%s"),
                *Intent.MarkerId);

            return;
        }

        UE_LOG(LogTemp, Warning,
            TEXT("SPAWNED CAMERA LABEL = %s"),
            *Cam->GetActorLabel()
        );

        Cam->MarkerId = Intent.MarkerId;
        Cam->ShotGuid = FGuid(Marker->MarkerGuid);

#if WITH_EDITOR
        Cam->SetActorLabel(
            CameraLabel,
            true
        );
#endif

        Intent.CameraActor = Cam;

        UE_LOG(LogTemp, Warning,
            TEXT("[GAS CameraPreview] Created blocking camera for promoted shot: %s"),
            *GetNameSafe(Cam));

        if (FGASMarker* MutableMarker =
            CurrentScript.Markers.FindByPredicate(
                [&](FGASMarker& M)
                {
                    return M.Id == Intent.MarkerId;
                }))
        {
            MutableMarker->bHasCamera = true;
            MutableMarker->CameraTransform = Cam->GetActorTransform();
        }
    }

    UE_LOG(LogTemp, Error,
        TEXT("[CAMERA] REAL CAMERA = %s"),
        *GetNameSafe(Cam));

    if (!IsValid(Cam))
    {
        UE_LOG(LogTemp, Error, TEXT("[GAS CameraPreview] Invalid camera actor. Skipping preview update."));
        return;
    }

    FVector PreviewLocation = FVector::ZeroVector;
    FRotator PreviewRotation = FRotator::ZeroRotator;
    float PreviewFocalLength = 50.f;

    BuildShotPreviewTransform(
        Intent,
        PreviewLocation,
        PreviewRotation,
        PreviewFocalLength
    );

    if (!IsValid(Cam))
    {
        UE_LOG(LogTemp, Error, TEXT("[CAM UPDATE] Cam invalid before SetActorLocation"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[CAM UPDATE] About to AddToRoot"));
    Cam->AddToRoot();

    UE_LOG(LogTemp, Warning, TEXT("[CAM UPDATE] About to SetActorLocation"));
    Cam->SetActorLocation(PreviewLocation);

    UE_LOG(LogTemp, Warning, TEXT("[CAM UPDATE] About to SetActorRotation"));
    Cam->SetActorRotation(PreviewRotation);

    UE_LOG(LogTemp, Warning, TEXT("[CAM UPDATE] About to check CineCamera"));
    if (IsValid(Cam) && IsValid(Cam->CineCamera))
    {
        UE_LOG(LogTemp, Warning, TEXT("[CAM UPDATE] About to SetCurrentFocalLength"));
        Cam->CineCamera->SetCurrentFocalLength(PreviewFocalLength);
        UE_LOG(LogTemp, Warning, TEXT("[CAM UPDATE] SetCurrentFocalLength done"));
    }

    UE_LOG(LogTemp, Warning, TEXT("[CAM UPDATE] About to RemoveFromRoot"));
    Cam->RemoveFromRoot();
    UE_LOG(LogTemp, Warning, TEXT("[CAM UPDATE] Done"));

    // --------------------------------------------------
    // Preview handled by SGAS_ShotIntentWindow only.
    // Do NOT initialize SGAS_ScriptTab preview here.
    // This prevents stale cross-world SceneCapture crashes.
    // --------------------------------------------------
    return;
}

void SGAS_ScriptTab::UpdateCameraFromShotIntentById(const FString& MarkerId)
{
    FGASShotIntent* Found = CurrentScript.ShotIntents.Find(MarkerId);
    if (!Found) return;

    // Copy by value — safe across potential map mutations
    FGASShotIntent IntentCopy = *Found;
    UpdateCameraFromShotIntent(IntentCopy);

    // Write back
    CurrentScript.ShotIntents.Add(MarkerId, IntentCopy);
}

void SGAS_ScriptTab::BuildShotPreviewTransform(
    const FGASShotIntent& Intent,
    FVector& OutLocation,
    FRotator& OutRotation,
    float& OutFocalLength
)
{
    if (!GEditor)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return;
    }

    if (Intent.SubjectId == TEXT("(No Subject)"))
    {
        return;
    }

    AGAS_StandInActor* TargetActor = nullptr;

    for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
    {
        if (!IsValid(*It)) continue;
        if (It->GetActorLabel().Contains(Intent.SubjectId))
        {
            TargetActor = *It;
            break;
        }
    }

    FVector TargetLocation = FVector(0, 0, 100);

    if (TargetActor)
    {
        TargetLocation = TargetActor->GetActorLocation();
        TargetLocation.Z += 140.f;
    }

    float Distance = GetShotDistance(Intent.ShotType);
    float HeightOffset = GetShotHeightOffset(Intent.ShotType);

    FVector CharacterForward = FVector(1.f, 0.f, 0.f);

    if (TargetActor)
    {
        UArrowComponent* Eyeline =
            TargetActor->FindComponentByClass<UArrowComponent>();

        if (Eyeline)
        {
            CharacterForward = Eyeline->GetForwardVector();
        }
        else
        {
            CharacterForward = TargetActor->GetActorForwardVector();
        }
    }

    FVector CameraDirection = CharacterForward;

    OutLocation =
        TargetLocation
        + (CameraDirection * Distance)
        + FVector(0.f, 0.f, HeightOffset);

    FVector LookAtTarget =
        TargetLocation
        + FVector(0.f, 0.f, HeightOffset);

    OutRotation =
        (LookAtTarget - OutLocation).Rotation();

    OutFocalLength = 50.f;
}


void SGAS_ScriptTab::RestoreCamerasAfterUndo()
{
    for (auto& KVP : CurrentScript.ShotIntents)
    {
        FGASShotIntent& Intent = KVP.Value;

        if (!IsValid(Intent.CameraActor))
        {
            // Only respawn if the marker is a blocking shot
            const FGASMarker* Marker =
                CurrentScript.Markers.FindByPredicate(
                    [&](const FGASMarker& M)
                    {
                        return M.Id == Intent.MarkerId;
                    }
                );

            if (Marker && Marker->IsBlocking())
            {
                UE_LOG(LogGASPrePro, Warning,
                    TEXT("[Undo] Respawning camera for marker: %s"),
                    *Intent.MarkerId);

                UpdateCameraFromShotIntent(Intent);
            }
        }
    }
}


void SGAS_ScriptTab::OpenShotIntentForMarker(
    const FString& MarkerId,
    const FString& SceneBlockId)
{

    // Prevent opening multiple Shot Intent windows
    if (bIsEditingShot)
    {
        UE_LOG(LogGASPrePro, Warning,
            TEXT("Shot Intent already open — ignoring"));
        return;
    }
    // Find the scene block
    FGASBlock* SceneBlock = nullptr;
    for (FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Id == SceneBlockId &&
            Block.Type == EGASBlockType::SceneHeading)
        {
            SceneBlock = &Block;
            break;
        }
    }

    if (!SceneBlock)
    {
        // No scene block found — open non-blocking
        if (ScriptPanel.IsValid())
            ScriptPanel->OpenShotIntentPopup(MarkerId, SceneBlockId, false, false);
        return;
    }

    const bool bHasBlocking = !SceneBlock->BlockingLevelPath.IsEmpty();

    if (!bHasBlocking)
    {
        // No blocking level — open non-blocking immediately
        if (ScriptPanel.IsValid())
            ScriptPanel->OpenShotIntentPopup(MarkerId, SceneBlockId, false, false);
        return;
    }

    // Blocking exists — is it already loaded?
    if (IsBlockingLevelOpen(SceneBlock->BlockingLevelPath))
    {
        // Already loaded — open immediately with blocking
        if (ScriptPanel.IsValid())
            ScriptPanel->OpenShotIntentPopup(MarkerId, SceneBlockId, true, false);
        return;
    }

    // Need to load the blocking level first
    // Store pending intent so HandleMapOpened can resume
    PendingShotIntentMarkerId = MarkerId;
    PendingShotIntentSceneId = SceneBlockId;

    ShowPendingActionWindow(TEXT("Opening blocking scene..."));
    OpenBlockingLevel(SceneBlock->BlockingLevelPath);
}

void SGAS_ScriptTab::Construct(const FArguments& InArgs)
{

    UE_LOG(LogGASPrePro, Verbose, TEXT("=== GAS SCRIPT TAB CONSTRUCTED ==="));

    GASBlockingAccess::SetBlockingActive(false);
    bBlockingActive = false;

    // --------------------------------------------------
    // Scene Numbering dropdown options
    // --------------------------------------------------
    SceneNumberingStyleOptions.Reset();
    SceneNumberingStyleOptions.Add(MakeShared<FString>(TEXT("By 10")));
    SceneNumberingStyleOptions.Add(MakeShared<FString>(TEXT("By 1")));
    SceneNumberingStyleOptions.Add(MakeShared<FString>(TEXT("Alphabetical")));

    // Default selection (match current Script.SceneNumbering if possible later)
    SceneNumberingStyleSelected = SceneNumberingStyleOptions[0];


    ChildSlot
        [
            SNew(SHorizontalBox)

                // =======================================================
                // LEFT PANEL — FIXED WIDTH
                // =======================================================
                +SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.f))
                        .Padding(FMargin(0.f, 0.f, 1.f, 0.f)) // 1px right border only
                        [
                            SNew(SBox)
                                .WidthOverride(56.f)
                                [
                                    SNew(SVerticalBox)

                                        // -----------------------------------------------
                                        // GROUP 1 — Shot Production
                                        // -----------------------------------------------

                                        // --- Shot Marker ---
                                        +SVerticalBox::Slot()
                                        .AutoHeight()
                                        .Padding(4.f, 8.f, 4.f, 4.f)
                                        [
                                            SNew(SBox)
                                                .WidthOverride(48.f)
                                                .HeightOverride(48.f)
                                                [
                                                    SNew(SButton)
                                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                        .HAlign(HAlign_Center)
                                                        .VAlign(VAlign_Center)
                                                        .OnClicked(this, &SGAS_ScriptTab::OnAddShotMarkerClicked)
                                                        .ToolTipText(FText::FromString("Shot Marker"))
                                                        [
                                                            SNew(SImage)
                                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.CameraIcon"))
                                                                .ColorAndOpacity(this, &SGAS_ScriptTab::GetShotButtonTint)
                                                        ]
                                                ]
                                        ]

                                    // --- Separator ---
                                    + SVerticalBox::Slot()
                                        .AutoHeight()
                                        .Padding(8.f, 4.f)
                                        [
                                            SNew(SSeparator)
                                                .Orientation(Orient_Horizontal)
                                                .Thickness(1.f)
                                        ]

                                        // -----------------------------------------------
                                        // GROUP 2 — View Toggles
                                        // -----------------------------------------------

                                        // --- Scene Numbers ---
                                        +SVerticalBox::Slot()
                                        .AutoHeight()
                                        .Padding(4.f, 4.f, 4.f, 4.f)
                                        [
                                            SNew(SBox)
                                                .WidthOverride(48.f)
                                                .HeightOverride(48.f)
                                                [
                                                    SNew(SButton)
                                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                        .HAlign(HAlign_Center)
                                                        .VAlign(VAlign_Center)
                                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleSceneNumbers))
                                                        .ToolTipText(FText::FromString("Toggle Scene Numbers"))
                                                        [
                                                            SNew(SImage)
                                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SceneNumberIcon"))
                                                                .ColorAndOpacity_Lambda([this]()
                                                                    {
                                                                        return bShowSceneNumbers
                                                                            ? FLinearColor(1.f, 0.8f, 0.2f, 1.f)   // amber when on
                                                                            : FLinearColor::White;
                                                                    })
                                                        ]
                                                ]
                                        ]

                                    // --- Separator ---
                                    + SVerticalBox::Slot()
                                        .AutoHeight()
                                        .Padding(8.f, 4.f)
                                        [
                                            SNew(SSeparator)
                                                .Orientation(Orient_Horizontal)
                                                .Thickness(1.f)
                                        ]

                                        // -----------------------------------------------
                                        // GROUP 3 — Script Editing
                                        // -----------------------------------------------

                                        // --- Edit Mode ---
                                        +SVerticalBox::Slot()
                                        .AutoHeight()
                                        .Padding(4.f, 4.f, 4.f, 4.f)
                                        [
                                            SNew(SBox)
                                                .WidthOverride(48.f)
                                                .HeightOverride(48.f)
                                                [
                                                    SNew(SButton)
                                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                        .HAlign(HAlign_Center)
                                                        .VAlign(VAlign_Center)
                                                        .IsEnabled_Lambda([this]() { return CurrentScript.Blocks.Num() > 0; })
                                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleEditMode))
                                                        .ToolTipText_Lambda([this]()
                                                            {
                                                                return bIsEditMode
                                                                    ? FText::FromString("Exit Edit Mode")
                                                                    : FText::FromString("Enter Edit Mode");
                                                            })
                                                        [
                                                            SNew(SImage)
                                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.EditIcon"))
                                                                .ColorAndOpacity_Lambda([this]()
                                                                    {
                                                                        return bIsEditMode
                                                                            ? FLinearColor(0.2f, 0.9f, 0.2f, 1.f)  // green when active
                                                                            : FLinearColor::White;
                                                                    })
                                                        ]
                                                ]
                                        ]

                                    // --- Add Mode ---
                                    + SVerticalBox::Slot()
                                        .AutoHeight()
                                        .Padding(4.f, 0.f, 4.f, 4.f)
                                        [
                                            SNew(SBox)
                                                .WidthOverride(48.f)
                                                .HeightOverride(48.f)
                                                [
                                                    SNew(SButton)
                                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                        .HAlign(HAlign_Center)
                                                        .VAlign(VAlign_Center)
                                                        .IsEnabled_Lambda([this]() { return CurrentScript.Blocks.Num() > 0; })
                                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleAddMode))
                                                        .ToolTipText_Lambda([this]()
                                                            {
                                                                return bIsAddMode
                                                                    ? FText::FromString("Exit Add Mode")
                                                                    : FText::FromString("Enter Add Mode");
                                                            })
                                                        [
                                                            SNew(SImage)
                                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.EditAddIcon"))
                                                                .ColorAndOpacity_Lambda([this]()
                                                                    {
                                                                        return bIsAddMode
                                                                            ? FLinearColor(0.2f, 0.9f, 0.2f, 1.f)  // green when active
                                                                            : FLinearColor::White;
                                                                    })
                                                        ]
                                                ]
                                        ]
                                ]
                        ]
                ]


            // =======================================================
            // MAIN PANELS (MIDDLE + RIGHT) INSIDE SPLITTER
            // =======================================================
                +SHorizontalBox::Slot()
                    .FillWidth(1.f)
                    [
                        SNew(SHorizontalBox)

                            // =======================================================
                            // SCRIPT PANEL (FIXED WIDTH)
                            // =======================================================
                            +SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                    .WidthOverride(ScriptPanelWidth)
                                    [
                                        SAssignNew(ScriptScrollBox, SScrollBox)
                                            .ScrollBarAlwaysVisible(true)
                                            .ConsumeMouseWheel(EConsumeMouseWheel::Always)
                                            .AllowOverscroll(EAllowOverscroll::No)
                                            .OnUserScrolled_Lambda(
                                                [this](float NewOffset)
                                                {
                                                    if (ScriptPanel.IsValid())
                                                    {
                                                        ScriptPanel->SetExternalScrollOffset(NewOffset);
                                                    }
                                                }
                                            )

                                            + SScrollBox::Slot()
                                            [
                                                SAssignNew(ScriptPanel, SGASScriptPanel)
                                                    .OwnerScriptTab(SharedThis(this))
                                            ]



                                    ]

                            ]

                        // =======================================================
                        // SHOT LIST PANEL (FIXED WIDTH)
                        // =======================================================
                        +SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SBox)
                                    .WidthOverride(ShotListPanelWidth)
                                    [
                                        SNew(SBorder)
                                            .Padding(8.f)
                                            [
                                                SNew(SScrollBox)
                                                    + SScrollBox::Slot()
                                                    [
                                                        SAssignNew(ShotListContainer, SVerticalBox)
                                                    ]
                                            ]
                                    ]
                            ]
                    ]

        ];

        FEditorDelegates::PreSaveWorldWithContext.AddLambda(
            [](UWorld* World, FObjectPreSaveContext)
            {
                GAS_ForceSequencerEval(World);
            }
        );

        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnRequestExternalScroll.BindLambda(
                [this](float NewOffset)
                {
                    if (!ScriptScrollBox.IsValid())
                    {
                        return;
                    }

                    // ScrollBox is the ONLY scroll authority
                    const float ViewOffset =
                        NewOffset - ScriptScrollBox->GetScrollOffset();

                    ScriptScrollBox->SetScrollOffset(ViewOffset + ScriptScrollBox->GetScrollOffset());

                }
            );
        }


        // --------------------------------------------------
        // Bind Shot List rebuild request from ScriptPanel
        // --------------------------------------------------
        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnRequestShotListRebuild.BindLambda(
                [this]()
                {
                    UE_LOG(LogGASPrePro, Warning, TEXT("[ShotListV2] Rebuild requested from ScriptPanel"));

                    const TArray<FRenderedParagraph>& Rendered = ScriptPanel->GetRenderedParagraphs();
                    for (const FRenderedParagraph& P : Rendered)
                    {
                        if (P.bStartsPage)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("[PB CHECK] Page=%d BlockId=%s TopY=%.1f"),
                                P.PageNumber, *P.BlockId, P.TopY);
                        }
                    }

                    RebuildShotList();
                }
            );
        }

        // ------------------------------------------------------------
        // Wire script mutation → project dirty state
        // ------------------------------------------------------------
        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnScriptMutated.BindLambda([this]()
                {
                    UE_LOG(LogGASPrePro, Warning, TEXT("SCRIPT TAB: Script mutated → marking dirty"));
                    MarkScriptDirty();
                    OnShotListNeedsRefresh.Broadcast();
                    if (ScriptPanel.IsValid())
                    {
                        ScriptPanel->RebuildLayout();
                        RebuildShotList();
                    }
                });


        }

        // ------------------------------------------------------------
        // Wire paragraph click → edit handling
        // ------------------------------------------------------------
        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnParagraphClicked =
                FOnParagraphClicked::CreateSP(
                    this, &SGAS_ScriptTab::OnScriptParagraphClicked
                );

        }

        if (ScriptPanel.IsValid())
        {
            ScriptPanel->OnShotActivated.BindRaw(
                this,
                &SGAS_ScriptTab::SetActiveBlockingShot
            );
        }

        RebuildShotList();
        LoadScriptFromJsonIfExists();

        if (ScriptPanel.IsValid())
        {
            ScriptPanel->SetScript(&CurrentScript);

            FGASSceneNumberingSettings EffectiveSceneNumbering =
                CurrentScript.SceneNumbering;


            if (ActiveProject)
            {
                const FGASProjectSettings& ProjectSettings =
                    ActiveProject->GetProjectSettings();

                EffectiveSceneNumbering.Style =
                    ProjectSettings.SceneNumberingStyle;

                EffectiveSceneNumbering.StartNumber =
                    ProjectSettings.SceneStartNumber;

 
            }


            TArray<FRenderedParagraph> Rendered =
                ScriptLayoutEngine::LayoutScript(
                    CurrentScript.Blocks,
                    CachedScriptPanelWidth,
                    CurrentScript.UserPageBreaks,
                    EffectiveSceneNumbering
                );

            ScriptPanel->SetRenderedScript(Rendered);
        }

        // --------------------------------------------------
        // Listen for level load completion
        // --------------------------------------------------
        OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddSP(
            this,
            &SGAS_ScriptTab::HandleMapOpened
        );

        OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddSP(
            this,
            &SGAS_ScriptTab::HandleMapOpened
        );

        // Reset blocking state on any map change
        FEditorDelegates::MapChange.AddSP(
            this,
            &SGAS_ScriptTab::OnMapChanged
        );

        RebuildShotList();
        RebuildCastUI();

        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateSP(
                this,
                &SGAS_ScriptTab::TickShotModeResume
            )
        );

}

void SGAS_ScriptTab::OnMapChanged(uint32 MapChangeFlags)
{
    if (bBlockingActive)
    {
        // If we're mid-flow creating or loading a blocking level, don't reset
        if (!PendingBlockingSceneId.IsEmpty() || bLoadingBlockingLevel)
        {
            return;
        }

        UE_LOG(LogGASPrePro, Warning, TEXT("Map changed while blocking active — resetting blocking state"));
        bBlockingActive = false;
        ActiveBlockingSceneId = FGuid();
        GASBlockingAccess::SetBlockingActive(false);
        GASBlockingAccess::SetActiveSceneId(FGuid());
        bPendingAutoOpenCastWindow = false;
        RebuildShotList();
    }
}

bool SGAS_ScriptTab::TickShotModeResume(float DeltaTime)
{
    if (!bPendingShotResumeAfterMapOpen)
    {
        return false;
    }

    if (PendingResumeSceneId.IsEmpty())
    {
        bPendingShotResumeAfterMapOpen = false;
        return false;
    }

    UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMode] Deferred resume executing Scene=%s"), *PendingResumeSceneId);

    const FString SceneId = PendingResumeSceneId;

    PendingResumeSceneId.Empty();
    bPendingShotResumeAfterMapOpen = false;

    bIsResumingShotMode = true;
    EnterShotMarkingMode(SceneId);
    bIsResumingShotMode = false;

    return false;
}

static FString ParagraphTypeToString(EGASBlockType Type)
{
    switch (Type)
    {
    case EGASBlockType::SceneHeading:   return TEXT("SceneHeading");
    case EGASBlockType::Action:         return TEXT("Action");
    case EGASBlockType::Character:      return TEXT("Character");
    case EGASBlockType::Dialogue:       return TEXT("Dialogue");
    case EGASBlockType::Parenthetical:  return TEXT("Parenthetical");
    case EGASBlockType::Transition:     return TEXT("Transition");
    default:                             return TEXT("Unknown");
    }
}

static EGASBlockType ParagraphTypeFromString(const FString& Str)
{
    if (Str == TEXT("SceneHeading"))   return EGASBlockType::SceneHeading;
    if (Str == TEXT("Action"))         return EGASBlockType::Action;
    if (Str == TEXT("Character"))      return EGASBlockType::Character;
    if (Str == TEXT("Dialogue"))       return EGASBlockType::Dialogue;
    if (Str == TEXT("Parenthetical"))  return EGASBlockType::Parenthetical;
    if (Str == TEXT("Transition"))     return EGASBlockType::Transition;
    return EGASBlockType::None;
}

static FString GetScriptJsonPath()
{
    const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GAS_Scripts"));
    IFileManager::Get().MakeDirectory(*Folder, /*Tree=*/true);
    return FPaths::Combine(Folder, TEXT("LastScript.json"));
}


// ============================================================================
// IMPORT SCRIPT — BUTTON HANDLER
// ============================================================================
FReply SGAS_ScriptTab::OnImportScript()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        return FReply::Handled();
    }


    // ------------------------------------------------------------
    // SAFETY WARNING — before opening file dialog
    // ------------------------------------------------------------
    if (CurrentScript.Blocks.Num() > 0)
    {
        const EAppReturnType::Type Response =
            FMessageDialog::Open(
                EAppMsgType::YesNo,
                FText::FromString(
                    TEXT("A script is already loaded.\n\n"
                        "ALL WORK WILL BE DESTROYED.\n\n"
                        "Are you sure you want to import a NEW script?")
                ),
                FText::FromString(TEXT("Import New Script?"))
            );

        if (Response != EAppReturnType::Yes)
        {
            return FReply::Handled();
        }

        bShotAddArmed = false;

        if (ScriptPanel.IsValid())
        {
            ScriptPanel->ResetEditorModes();
            ScriptPanel->SetShotAddArmed(false);
        }
    }


    TArray<FString> OutFiles;

    const void* ParentWindow =
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

    bool bOpened = DesktopPlatform->OpenFileDialog(
        ParentWindow,
        TEXT("Select Final Draft Script"),
        FPaths::ProjectDir(),
        TEXT(""),
        TEXT("Final Draft Files (*.fdx)|*.fdx"),
        EFileDialogFlags::None,
        OutFiles
    );

    if (bOpened && OutFiles.Num() > 0)
    {
        // Import replaces the script wholesale
        FGASImportNumberingOptions ImportOptions;

        ImportOptions.SceneNumberingStyle =
            PendingProjectSettings.SceneNumberingStyle;

        ImportOptions.ShotNumberingPolicy =
            PendingProjectSettings.ShotNumberingPolicy;

        ImportOptions.bRespectScriptSceneNumbers =
            (
                PendingProjectSettings.SceneNumberingStyle ==
                EGASNumberingStyle::FromScript
                );

        LoadScriptFromFDX(
            OutFiles[0],
            ImportOptions
        );
        RebuildShotList();

        // ------------------------------------------------------------
        // AUTO CREATE MASTER SEQUENCE
        // ------------------------------------------------------------
        if (ActiveProject)
        {
            UE_LOG(LogGASPrePro, Warning,
                TEXT("[AutoMaster] Starting auto master sequence pipeline..."));

            // Step 1 — Generate page breaks if none exist
            OnGeneratePageBreaks();

            // Step 2 — Rebuild layout so eighths are accurate
            if (ScriptPanel.IsValid())
            {
                ScriptPanel->RebuildLayout();
            }

            // Step 3 — Place scene markers on the master timeline
            BuildSceneMarkersFromScriptTiming();

            UE_LOG(LogGASPrePro, Warning,
                TEXT("[AutoMaster] Pipeline complete."));
        }
    }
    return FReply::Handled();
}


// ============================================================================
// SAVE SCRIPT — BUTTON HANDLER
// ============================================================================

FReply SGAS_ScriptTab::OnSaveScript()
{
    UE_LOG(LogTemp, Warning, TEXT("=== SAVE START ==="));

    SaveScriptToJson();



#if WITH_EDITOR
    if (GEditor)
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();

        if (World)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Saving LEVEL package"));

            World->MarkPackageDirty();

            UPackage* LevelPackage = World->PersistentLevel
                ? World->PersistentLevel->GetOutermost()
                : nullptr;

            if (LevelPackage)
            {
                FString Filename =
                    FPackageName::LongPackageNameToFilename(
                        LevelPackage->GetName(),
                        FPackageName::GetMapPackageExtension()
                    );

                FSavePackageArgs SaveArgs;
                SaveArgs.TopLevelFlags = RF_Standalone;
                SaveArgs.Error = GError;
                SaveArgs.SaveFlags = SAVE_None;

                //bool bSaved = UPackage::SavePackage(
                //    LevelPackage,
                //    nullptr,
                //    *Filename,
                //    SaveArgs
                //);

                UEditorLoadingAndSavingUtils::SaveDirtyPackages(
                    true,
                    true
                );

                UE_LOG(LogTemp, Warning,
                    TEXT("LEVEL SAVE REQUESTED"));
            }
        }
    }
#endif

    UE_LOG(LogTemp, Warning, TEXT("=== SAVE END ==="));

    return FReply::Handled();
}

FReply SGAS_ScriptTab::OnToggleSceneNumbers()
{
    bShowSceneNumbers = !bShowSceneNumbers;

    UE_LOG(LogGASPrePro, Verbose,
        TEXT("SCRIPT TAB: Scene numbers %s"),
        bShowSceneNumbers ? TEXT("ON") : TEXT("OFF"));

    float SavedScrollOffset = 0.f;

    if (ScriptScrollBox.IsValid())
    {
        SavedScrollOffset = ScriptScrollBox->GetScrollOffset();
    }

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetShowSceneNumbers(bShowSceneNumbers);
        ScriptPanel->RebuildLayout();
    }

    // 🔒 Restore scroll NEXT TICK (after Slate layout settles)
    if (ScriptScrollBox.IsValid())
    {
        TWeakPtr<SScrollBox> WeakScrollBox = ScriptScrollBox;
        const float OffsetToRestore = SavedScrollOffset;

        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([WeakScrollBox, OffsetToRestore](float)
                {
                    if (WeakScrollBox.IsValid())
                    {
                        WeakScrollBox.Pin()->SetScrollOffset(OffsetToRestore);
                    }
                    return false; // one-shot
                })
        );
    }

    return FReply::Handled();
}

void SGAS_ScriptTab::PromptCreateNewProject()
{

    TSharedPtr<SEditableTextBox> NameTextBox;

    NewProjectAspectOptions.Reset();
    NewProjectAspectOptions.Add(MakeShared<FString>(TEXT("16:9")));
    NewProjectAspectOptions.Add(MakeShared<FString>(TEXT("2.39:1")));
    NewProjectAspectOptions.Add(MakeShared<FString>(TEXT("4:3")));
    NewProjectAspectOptions.Add(MakeShared<FString>(TEXT("1:1")));

    NewProjectFrameRateOptions.Reset();
    NewProjectFrameRateOptions.Add(MakeShared<FString>(TEXT("23.976")));
    NewProjectFrameRateOptions.Add(MakeShared<FString>(TEXT("24")));
    NewProjectFrameRateOptions.Add(MakeShared<FString>(TEXT("25")));
    NewProjectFrameRateOptions.Add(MakeShared<FString>(TEXT("29.97")));
    NewProjectFrameRateOptions.Add(MakeShared<FString>(TEXT("30")));

    NewProjectSceneNumberingOptions.Reset();

    NewProjectSceneNumberingOptions.Add(
        MakeShared<FString>(TEXT("By 10"))
    );

    NewProjectSceneNumberingOptions.Add(
        MakeShared<FString>(TEXT("By 1"))
    );

    NewProjectSceneNumberingOptions.Add(
        MakeShared<FString>(TEXT("Alphabetic"))
    );

    NewProjectSceneNumberingOptions.Add(
        MakeShared<FString>(TEXT("Use FDX"))
    );

    NewProjectShotNumberingOptions.Reset();

    NewProjectShotNumberingOptions.Add(
        MakeShared<FString>(TEXT("By 1"))
    );

    NewProjectShotNumberingOptions.Add(
        MakeShared<FString>(TEXT("By 10"))
    );

    NewProjectShotNumberingOptions.Add(
        MakeShared<FString>(TEXT("Alphabetic"))
    );

    PendingProjectSettings = FGASProjectSettings();

    NewProjectSelectedAspect = NewProjectAspectOptions[0];
    PendingProjectSettings.AspectRatio = *NewProjectSelectedAspect;
    NewProjectSelectedFrameRate = NewProjectFrameRateOptions[1];
    PendingProjectSettings.FrameRate =
        EGASProjectFrameRate::FPS_24;

    NewProjectSelectedSceneNumbering =
        NewProjectSceneNumberingOptions[0];

    PendingProjectSettings.SceneNumberingStyle =
        EGASNumberingStyle::CountBy10;

    NewProjectSelectedShotNumbering =
        NewProjectShotNumberingOptions[0];

    PendingProjectSettings.ShotNumberingPolicy =
        EGASShotNumberingPolicy::Numeric_1s;
    PendingProjectSettings.ShotStartNumber = 1;

    TSharedRef<SWindow> CreateWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Create New GAS Project")))
        .ClientSize(FVector2D(400.f, 520.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    CreateWindow->SetContent(
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(10.f, 10.f, 10.f, 8.f)
        [
            SNew(SBorder)
                .Padding(FMargin(8.f, 6.f))
                .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("PROJECT")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
        ]

    + SVerticalBox::Slot()
        .Padding(10.f, 2.f, 10.f, 5.f)
        .AutoHeight()
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Project Name")))
        ]

        + SVerticalBox::Slot()
        .Padding(10.f, 0.f, 10.f, 10.f)
        .AutoHeight()
        [
            SAssignNew(NameTextBox, SEditableTextBox)
        ]

        + SVerticalBox::Slot()
        .Padding(10.f, 5.f, 10.f, 10.f)
        .AutoHeight()
        [
            SNew(SHorizontalBox)

                // =====================================================
                // Aspect Ratio
                // =====================================================

                +SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.f, 0.f, 16.f, 0.f)
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 0.f, 0.f, 4.f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Aspect Ratio")))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                                .WidthOverride(140.f)
                                [
                                    SNew(SComboBox<TSharedPtr<FString>>)
                                        .OptionsSource(&NewProjectAspectOptions)
                                        .InitiallySelectedItem(NewProjectSelectedAspect)

                                        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                                            {
                                                return SNew(STextBlock)
                                                    .Text(
                                                        Item.IsValid()
                                                        ? FText::FromString(*Item)
                                                        : FText::GetEmpty()
                                                    );
                                            })

                                        .OnSelectionChanged(
                                            this,
                                            &SGAS_ScriptTab::OnNewProjectAspectChanged
                                        )

                                        [
                                            SNew(STextBlock)
                                                .Text(
                                                    this,
                                                    &SGAS_ScriptTab::GetNewProjectAspectText
                                                )
                                        ]
                                ]
                        ]
                ]

            // =====================================================
            // Frame Rate
            // =====================================================

            +SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 0.f, 0.f, 4.f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Frame Rate")))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                                .WidthOverride(120.f)
                                [
                                    SNew(SComboBox<TSharedPtr<FString>>)
                                        .OptionsSource(&NewProjectFrameRateOptions)
                                        .InitiallySelectedItem(NewProjectSelectedFrameRate)

                                        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                                            {
                                                return SNew(STextBlock)
                                                    .Text(
                                                        Item.IsValid()
                                                        ? FText::FromString(*Item)
                                                        : FText::GetEmpty()
                                                    );
                                            })

                                        .OnSelectionChanged(
                                            this,
                                            &SGAS_ScriptTab::OnNewProjectFrameRateChanged
                                        )

                                        [
                                            SNew(STextBlock)
                                                .Text(
                                                    this,
                                                    &SGAS_ScriptTab::GetNewProjectFrameRateText
                                                )
                                        ]
                                ]
                        ]
                ]
        ]

    + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(10.f, 10.f, 10.f, 8.f)
        [
            SNew(SBorder)
                .Padding(FMargin(8.f, 6.f))
                .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("NUMBERING")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
        ]

    + SVerticalBox::Slot()
        .Padding(10.f, 2.f, 10.f, 10.f)
        .AutoHeight()
        [
            SNew(SHorizontalBox)

                // =====================================================
                // Scene Numbering
                // =====================================================

                +SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.f, 0.f, 16.f, 0.f)
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 0.f, 0.f, 4.f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Scene Numbering")))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                                .WidthOverride(160.f)
                                [
                                    SNew(SComboBox<TSharedPtr<FString>>)
                                        .OptionsSource(&NewProjectSceneNumberingOptions)
                                        .InitiallySelectedItem(NewProjectSelectedSceneNumbering)

                                        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                                            {
                                                return SNew(STextBlock)
                                                    .Text(
                                                        Item.IsValid()
                                                        ? FText::FromString(*Item)
                                                        : FText::GetEmpty()
                                                    );
                                            })

                                        .OnSelectionChanged(
                                            this,
                                            &SGAS_ScriptTab::OnNewProjectSceneNumberingChanged
                                        )

                                        [
                                            SNew(STextBlock)
                                                .Text(
                                                    this,
                                                    &SGAS_ScriptTab::GetNewProjectSceneNumberingText
                                                )
                                        ]
                                ]
                        ]
                ]

            // =====================================================
            // Shot Numbering
            // =====================================================

            +SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 0.f, 0.f, 4.f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Shot Numbering")))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                                .WidthOverride(160.f)
                                [
                                    SNew(SComboBox<TSharedPtr<FString>>)
                                        .OptionsSource(&NewProjectShotNumberingOptions)
                                        .InitiallySelectedItem(NewProjectSelectedShotNumbering)

                                        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                                            {
                                                return SNew(STextBlock)
                                                    .Text(
                                                        Item.IsValid()
                                                        ? FText::FromString(*Item)
                                                        : FText::GetEmpty()
                                                    );
                                            })

                                        .OnSelectionChanged(
                                            this,
                                            &SGAS_ScriptTab::OnNewProjectShotNumberingChanged
                                        )

                                        [
                                            SNew(STextBlock)
                                                .Text(
                                                    this,
                                                    &SGAS_ScriptTab::GetNewProjectShotNumberingText
                                                )
                                        ]
                                ]
                        ]
                ]
        ]

        + SVerticalBox::Slot()
            .Padding(10.f, 0.f, 10.f, 10.f)
            .AutoHeight()
            [
                SNew(SHorizontalBox)

                    // =====================================================
                    // Scene Start Number
                    // =====================================================

                    +SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0.f, 0.f, 16.f, 0.f)
                    [
                        SNew(SVerticalBox)

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.f, 0.f, 0.f, 4.f)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Scene Start")))
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(SceneStartNumberSpinBox, SSpinBox<int32>)
                                    .MinValue(1)
                                    .MaxValue(9999)
                                    .Value(PendingProjectSettings.SceneStartNumber)
                                    .MinDesiredWidth(120.f)
                                    .OnValueChanged(
                                        this,
                                        &SGAS_ScriptTab::OnSceneStartNumberChanged
                                    )
                            ]
                    ]

                // =====================================================
                // Shot Start Number
                // =====================================================

                +SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SVerticalBox)

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.f, 0.f, 0.f, 4.f)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Shot Start")))
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                SAssignNew(ShotStartNumberSpinBox, SSpinBox<int32>)
                                    .MinValue(1)
                                    .MaxValue(9999)
                                    .Value(PendingProjectSettings.ShotStartNumber)
                                    .MinDesiredWidth(120.f)
                                    .OnValueChanged(
                                        this,
                                        &SGAS_ScriptTab::OnShotStartNumberChanged
                                    )
                            ]
                    ]
            ]


        + SVerticalBox::Slot()
        .Padding(10.f)
        .HAlign(HAlign_Right)
        .AutoHeight()
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.f, 0.f, 5.f, 0.f)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Create")))
                        .OnClicked_Lambda([this, NameTextBox, CreateWindow]()
                            {
                                if (!NameTextBox.IsValid())
                                {
                                    return FReply::Handled();
                                }

                                const FString EnteredName = NameTextBox->GetText().ToString().TrimStartAndEnd();

                                if (EnteredName.IsEmpty())
                                {
                                    return FReply::Handled();
                                }

                                if (CreateNewProject(
                                    EnteredName,
                                    PendingProjectSettings))
                                {
                                    // Reset script state
                                    CurrentScript = FGASScript();

                                    // 🔹 IMPORTANT — notify panel of new script
                                    if (ScriptPanel.IsValid())
                                    {
                                        ScriptPanel->SetScript(&CurrentScript);
                                    }

                                    RebuildShotList();
                                    RebuildCastUI();

                                    FSlateApplication::Get().RequestDestroyWindow(CreateWindow);
                                }

                                return FReply::Handled();
                            })
                ]

            + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Cancel")))
                        .OnClicked_Lambda([CreateWindow]()
                            {
                                FSlateApplication::Get().RequestDestroyWindow(CreateWindow);
                                return FReply::Handled();
                            })
                ]
        ]
    );

    FSlateApplication::Get().AddWindow(CreateWindow);
}

void SGAS_ScriptTab::PromptOpenProject()
{
    FAssetPickerConfig PickerConfig;
    PickerConfig.Filter.ClassPaths.Add(UGASPreProProject::StaticClass()->GetClassPathName());
    PickerConfig.SelectionMode = ESelectionMode::Single;
    PickerConfig.bAllowNullSelection = false;

    TSharedRef<SWindow> PickerWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Open GAS Project")))
        .ClientSize(FVector2D(600, 400))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    PickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(
        [this, PickerWindow](const FAssetData& AssetData)
        {
            FString AssetPath = AssetData.GetObjectPathString();

            if (LoadProject(AssetPath))
            {
                UE_LOG(LogGASPrePro, Verbose, TEXT("Loaded Project via Picker"));
            }

            FSlateApplication::Get().RequestDestroyWindow(PickerWindow);
        }
    );

    PickerWindow->SetContent(
        FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser")
        .Get()
        .CreateAssetPicker(PickerConfig)
    );

    FSlateApplication::Get().AddWindow(PickerWindow);
}


FGASBlock* SGAS_ScriptTab::GetSceneBlockFromCursor()
{
    if (!ScriptPanel.IsValid())
        return nullptr;

    const FString BlockId = ScriptPanel->GetSelectedBlockId();

    UE_LOG(LogTemp, Warning, TEXT("Selected BlockId: %s"), *BlockId);

    if (BlockId.IsEmpty())
        return nullptr;

    return GetSceneBlockById(BlockId);
}

FGASBlock* SGAS_ScriptTab::GetSceneBlockById(const FString& BlockId)
{
    int32 FoundIndex = INDEX_NONE;

    // --------------------------------------------------
    // Find the block index first
    // --------------------------------------------------
    for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
    {
        if (CurrentScript.Blocks[i].Id == BlockId)
        {
            FoundIndex = i;
            break;
        }
    }

    if (FoundIndex == INDEX_NONE)
        return nullptr;

    // --------------------------------------------------
    // Walk backwards to find Scene Heading
    // --------------------------------------------------
    for (int32 i = FoundIndex; i >= 0; --i)
    {
        FGASBlock& Block = CurrentScript.Blocks[i];

        if (Block.Type == EGASBlockType::SceneHeading)
        {
            return &Block;
        }
    }

    return nullptr;
}

FReply SGAS_ScriptTab::OnAddShotMarkerClicked()
{
    bShotAddArmed = !bShotAddArmed;

    UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMarker] Shot mode = %s"),
        bShotAddArmed ? TEXT("ON") : TEXT("OFF"));

    // --------------------------------------------------
    // Reset scene tracking when turning ON
    // --------------------------------------------------
    if (bShotAddArmed)
    {
        ActiveCameraModeSceneId.Empty();
    }

    // --------------------------------------------------
    // Notify panel (INPUT ONLY)
    // --------------------------------------------------
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetShotAddArmed(bShotAddArmed);
    }

    // --------------------------------------------------
    // Force UI refresh (THIS MATTERS)
    // --------------------------------------------------
    Invalidate(EInvalidateWidget::LayoutAndVolatility);

    return FReply::Handled();
}


bool SGAS_ScriptTab::CreateNewProject(
    const FString& ProjectName,
    const FGASProjectSettings& InProjectSettings)
{
    if (ProjectName.IsEmpty())
    {
        return false;
    }

    FString CleanName = ProjectName;
    CleanName.ReplaceInline(TEXT(" "), TEXT("_"));

    const FString PackagePath =
        FString::Printf(
            TEXT("/Game/PrePro/Projects/%s"),
            *CleanName
        );

    const FString AssetName = CleanName;
    const FString FullPackagePath = PackagePath + TEXT("/") + AssetName;

    // ------------------------------------------------------------
    // If project already exists on disk, load it instead
    // ------------------------------------------------------------
    const FString ExistingFilePath =
        FPackageName::LongPackageNameToFilename(
            FullPackagePath,
            FPackageName::GetAssetPackageExtension()
        );

    if (FPaths::FileExists(ExistingFilePath))
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("Project already exists. Loading instead: %s"), *CleanName);

        return LoadProject(FullPackagePath + TEXT(".") + AssetName);
    }

    // ------------------------------------------------------------
    // Create new project asset
    // ------------------------------------------------------------
    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        return false;
    }

    Package->FullyLoad();

    UGASPreProProject* NewProject =
        NewObject<UGASPreProProject>(
            Package,
            UGASPreProProject::StaticClass(),
            *AssetName,
            RF_Public | RF_Standalone
        );
    NewProject->ProjectSettings.AspectRatio =
        InProjectSettings.AspectRatio;

    NewProject->ProjectSettings.FrameRate =
        InProjectSettings.FrameRate;

    NewProject->ProjectSettings.SceneNumberingStyle =
        InProjectSettings.SceneNumberingStyle;

    NewProject->ProjectSettings.SceneStartNumber =
        InProjectSettings.SceneStartNumber;

    NewProject->ProjectSettings.ShotNumberingPolicy =
        InProjectSettings.ShotNumberingPolicy;

    NewProject->ProjectSettings.ShotStartNumber =
        InProjectSettings.ShotStartNumber;

    NewProject->ProjectSettings.bEnableBlocking = true;
    NewProject->ProjectSettings.bAutoCreateMasterSequence = true;

    NewProject->MarkPackageDirty();

    if (!NewProject)
    {
        return false;
    }

    // Assign name
    NewProject->ProjectName = CleanName;

    // Mark dirty and register
    FAssetRegistryModule::AssetCreated(NewProject);

    // Save package to disk
    const FString FilePath =
        FPackageName::LongPackageNameToFilename(
            FullPackagePath,
            FPackageName::GetAssetPackageExtension()
        );

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_None;

    bool bSaved = UPackage::SavePackage(
        Package,
        NewProject,
        *FilePath,
        SaveArgs
    );

    if (!bSaved)
    {
        return false;
    }

    // Set active project
    ActiveProject = NewProject;

    // ------------------------------------------------------------
    // Immediately prompt to import script after project creation
    // ------------------------------------------------------------
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float)
            {
                OnImportScript();
                return false; // run once
            })
    );

    return true;
}

bool SGAS_ScriptTab::LoadProject(const FString& AssetPath)
{
    if (AssetPath.IsEmpty())
    {
        return false;
    }
    CurrentScript = FGASScript();

    UGASPreProProject* LoadedProject =
        LoadObject<UGASPreProProject>(
            nullptr,
            *AssetPath
        );

    if (!LoadedProject)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Failed to load project: %s"), *AssetPath);
        return false;
    }

    ActiveProject = LoadedProject;

    UE_LOG(LogGASPrePro, Verbose, TEXT("ACTIVE PROJECT POINTER: %p"), ActiveProject);

    UE_LOG(
        LogGASPrePro,
        Verbose,
        TEXT("PROJECT LOADED: %s"),
        *ActiveProject->ProjectName
    );

    // Load script JSON for this project
    LoadScriptFromJsonIfExists();

    // Rebuild UI
    RebuildShotList();
    RebuildCastUI();

    return true;
}

// ============================================================================
// LOAD SCRIPT (.FDX) — parse, layout, send to panel
// ============================================================================
void SGAS_ScriptTab::LoadScriptFromFDX(
    const FString& FilePath,
    const FGASImportNumberingOptions& ImportOptions
)

{
    // --------------------------------------------------------------------
    // 0. Confirm overwrite if script already loaded
    // --------------------------------------------------------------------
    if (CurrentScript.Blocks.Num() > 0)

    {
        const FText Title = FText::FromString("Replace script?");
        const FText Message = FText::FromString(
            "Are you sure you want to load a new script?\n"
            "This will overwrite the current script, page breaks, and markers."
        );

        if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title)
            == EAppReturnType::No)
        {
            return;
        }
    }

    // --------------------------------------------------------------------
    // 1. Clear previous script state
    // --------------------------------------------------------------------
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetRenderedScript({});
        ScriptPanel->SetShotMarkers({});
    }

    RebuildShotList();

    // --------------------------------------------------------------------
    // 2. Import FDX into a temporary FGASScript
    // --------------------------------------------------------------------
    FGASScript TempScript;

    // Import options now come directly from the import dialog
    const FGASImportNumberingOptions& Options = ImportOptions;



    if (!UGAS_FDXImporter::ImportFDXToScript(FilePath, TempScript, Options))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("FDX import failed: %s"), *FilePath);
        return;
    }




    // --------------------------------------------------------------------
    // 4. Populate authoritative in-memory script (JSON-backed)
    // --------------------------------------------------------------------
    CurrentScript = TempScript;
    BuildCastListFromScript();
    RebuildCastUI();

    UE_LOG(
        LogGASPrePro,
        Warning,
        TEXT("[IMPORT VERIFY] Scene BaseStyle = %d  Style = %d"),
        (int32)CurrentScript.SceneNumbering.BaseStyle,
        (int32)CurrentScript.SceneNumbering.Style
    );
    // ------------------------------------------------------------
    // Persist shot numbering policy into script (CRITICAL)
    // ------------------------------------------------------------
    if (ActiveProject)
    {
        const FGASProjectSettings& ProjectSettings =
            ActiveProject->GetProjectSettings();

        CurrentScript.ShotNumberingPolicy =
            ProjectSettings.ShotNumberingPolicy;

        CurrentScript.SceneNumbering.Style =
            ProjectSettings.SceneNumberingStyle;

        CurrentScript.SceneNumbering.StartNumber =
            ProjectSettings.SceneStartNumber;

        CurrentScript.ShotStartNumber =
            ProjectSettings.ShotStartNumber;
    }
    else
    {
        CurrentScript.ShotNumberingPolicy =
            ImportOptions.ShotNumberingPolicy;
    }



    CurrentScript.UserPageBreaks.Empty();


    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetScript(&CurrentScript);
    }

    UE_LOG(
        LogGASPrePro,
        Warning,
        TEXT("AFTER IMPORT: Blocks=%d  PageBreaks=%d"),
        CurrentScript.Blocks.Num(),
        CurrentScript.UserPageBreaks.Num()
    );


    // --------------------------------------------------------------------
    // 5. Layout script
    // --------------------------------------------------------------------
    float PanelWidth = 1100.f;
    if (ScriptPanel.IsValid())
    {
        PanelWidth = ScriptPanel->GetCachedGeometry().GetLocalSize().X;
        if (PanelWidth < 200.f)
            PanelWidth = 1100.f;
    }

    FGASSceneNumberingSettings EffectiveSceneNumbering =
        CurrentScript.SceneNumbering;

    if (ActiveProject)
    {
        const FGASProjectSettings& ProjectSettings =
            ActiveProject->GetProjectSettings();

        EffectiveSceneNumbering.Style =
            ProjectSettings.SceneNumberingStyle;

        EffectiveSceneNumbering.StartNumber =
            ProjectSettings.SceneStartNumber;

    }

    TArray<FRenderedParagraph> Rendered =
        ScriptLayoutEngine::LayoutScript(
            CurrentScript.Blocks,
            PanelWidth,
            CurrentScript.UserPageBreaks,
            EffectiveSceneNumbering
        );

    ScriptPanel->SetRenderedScript(Rendered);

    TArray<FGASShotListSceneRow> SceneRowsV2;

    BuildShotListFromScriptV2(
        CurrentScript,
        EffectiveSceneNumbering,
        Rendered,
        SceneRowsV2
    );



    // --------------------------------------------------------------------
    // 6. Clear markers (FDX has none)
    // --------------------------------------------------------------------

    RebuildShotList();
    MarkScriptDirty();
    OnSaveScript();

}


// NOTE:
// Auto page breaks are a bootstrap helper.
// Final layout uses semantic page breaks only.
// Tune ApproxLinesPerPage (≈53) for Final Draft parity.


FReply SGAS_ScriptTab::OnGeneratePageBreaks()
{
    // One-time bootstrap only
    if (CurrentScript.UserPageBreaks.Num() > 0)
    {
        return FReply::Handled();
    }

    int32 LinesOnPage = 0;
    const int32 ApproxLinesPerPage = 54;

    for (int32 BlockIndex = 0; BlockIndex < CurrentScript.Blocks.Num(); ++BlockIndex)
    {
        const FGASBlock& Block = CurrentScript.Blocks[BlockIndex];

        // Ignore right-side dual dialogue
        if (Block.bIsDualDialogue && Block.DualRole == EGASDualRole::Right)
        {
            continue;
        }

        // VERY rough estimate (fine for bootstrap)
        // Estimate line usage based on block type (bootstrap only)
        int32 EstimatedLines = 0;

        switch (Block.Type)
        {
        case EGASBlockType::SceneHeading:
            EstimatedLines = 2;
            break;

        case EGASBlockType::Action:
            EstimatedLines = 3;
            break;

        case EGASBlockType::Character:
            EstimatedLines = 1;
            break;

        case EGASBlockType::Dialogue:
            EstimatedLines = 2;
            break;

        case EGASBlockType::Parenthetical:
            EstimatedLines = 1;
            break;

        case EGASBlockType::Transition:
            EstimatedLines = 1;
            break;

        default:
            EstimatedLines = 1;
            break;
        }

        LinesOnPage += EstimatedLines;


        if (LinesOnPage >= ApproxLinesPerPage)
        {
            FGASUserPageBreak NewBreak;
            NewBreak.AfterBlockId = Block.Id;
            CurrentScript.UserPageBreaks.Add(NewBreak);

            LinesOnPage = 0; // reset for next page
        }
    }

    // Let normal pipeline rebuild layout
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->RebuildLayout();
    }

    RebuildShotList();
    return FReply::Handled();
    

}

void SGAS_ScriptTab::EnsureScriptSaved()
{
    if (CurrentScript.Blocks.Num() == 0)
    {
        return;
    }

    OnSaveScript();
}

void SGAS_ScriptTab::SaveScriptJsonOnly()
{
    if (CurrentScript.Blocks.Num() == 0)
        return;

    SaveScriptToJson();
}

void SGAS_ScriptTab::MarkScriptDirty()
{
    bIsScriptDirty = true;

    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.MarkToolDirty();
}

void SGAS_ScriptTab::ClearScriptDirty()
{
    bIsScriptDirty = false;
}

bool SGAS_ScriptTab::IsScriptDirty() const
{
    return bIsScriptDirty;
}

void SGAS_ScriptTab::ClearScript()
{
    // Clear authoritative in-memory script
    CurrentScript = FGASScript();

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetScript(&CurrentScript);
        ScriptPanel->SetRenderedScript({});
    }

    // ✅ Force the parent ScrollBox (the real scroll owner) back to top
    if (ScriptScrollBox.IsValid())
    {
        ScriptScrollBox->SetScrollOffset(0.f);
    }

    RebuildShotList();

    UE_LOG(LogGASPrePro, Verbose, TEXT("SCRIPT CLEARED"));
}

FReply SGAS_ScriptTab::OnToggleEditMode()
{
    bIsEditMode = !bIsEditMode;


    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetEditMode(bIsEditMode);
    }
    else
    {
        UE_LOG(LogGASPrePro, Error, TEXT("EDIT MODE: ScriptPanel INVALID"));
    }

    return FReply::Handled();
}

void SGAS_ScriptTab::BuildCastListFromScript()
{
    CastList.Empty();

    for (const FGASCharacterDefinition& Def : CurrentScript.Cast)
    {
        FGASCastMember Member;
        Member.Name = Def.CharacterName;
        Member.bEnabled = Def.bEnabled;

        CastList.Add(Member);
    }
}

void SGAS_ScriptTab::RebuildCastUI()
{
    if (!CastListContainer.IsValid())
        return;

    CastListContainer->ClearChildren();

    if (CastList.Num() == 0)
    {
        CastListContainer->AddSlot()
            .AutoHeight()
            .Padding(4.f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("No characters found")))
                    .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
            ];
        return;
    }

    for (int32 MemberIndex = 0; MemberIndex < CastList.Num(); ++MemberIndex)
    {
        const FString MemberName = CastList[MemberIndex].Name;

        CastListContainer->AddSlot()
            .AutoHeight()
            .Padding(2.f)
            [
                SNew(SCheckBox)
                    .IsChecked_Lambda([this, MemberIndex]()
                        {
                            if (!CastList.IsValidIndex(MemberIndex))
                            {
                                return ECheckBoxState::Unchecked;
                            }

                            return CastList[MemberIndex].bEnabled
                                ? ECheckBoxState::Checked
                                : ECheckBoxState::Unchecked;
                        })
                    .OnCheckStateChanged_Lambda([this, MemberIndex](ECheckBoxState State)
                        {
                            if (!CastList.IsValidIndex(MemberIndex))
                            {
                                return;
                            }

                            CastList[MemberIndex].bEnabled =
                                (State == ECheckBoxState::Checked);
                        })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(MemberName))
                    ]
            ];
    }
}

bool SGAS_ScriptTab::GetSceneCastForBlockId(const FString& SceneBlockId, TArray<FString>& OutSceneCast) const
{
    OutSceneCast.Reset();

    if (SceneBlockId.IsEmpty())
    {
        return false;
    }

    for (const FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Id == SceneBlockId)
        {
            if (Block.CharactersInScene.Num() <= 0)
            {
                return false;
            }

            UE_LOG(LogGASPrePro, Warning,
                TEXT("SCENE FILTER: %s → %d characters"),
                *SceneBlockId,
                Block.CharactersInScene.Num());

            OutSceneCast.Reserve(Block.CharactersInScene.Num());

            for (const FString& C : Block.CharactersInScene)
            {
                const FString Clean = C.TrimStartAndEnd();
                if (!Clean.IsEmpty())
                {
                    OutSceneCast.Add(Clean);
                }
            }

            return OutSceneCast.Num() > 0;
        }
    }

    return false;
}

void SGAS_ScriptTab::GetEnabledCastNames_SceneAware(TArray<TSharedPtr<FString>>& OutNames) const
{
    OutNames.Reset();

    // 1) Get global enabled cast (script-level cast)
    TArray<TSharedPtr<FString>> EnabledGlobal;
    EnabledGlobal.Reserve(CastList.Num());

    for (const FGASCastMember& M : CastList)
    {
        if (!M.bEnabled)
        {
            continue;
        }

        const FString Clean = M.Name.TrimStartAndEnd();
        if (!Clean.IsEmpty())
        {
            EnabledGlobal.Add(MakeShared<FString>(Clean));
        }
    }

    // 2) Resolve "active scene" from current ScriptPanel selection
    FString SceneBlockId;
    if (ScriptPanel.IsValid())
    {
        SceneBlockId = ScriptPanel->GetSelectedBlockId();
    }

    // 3) If no active scene, return global
    if (SceneBlockId.IsEmpty())
    {
        OutNames = MoveTemp(EnabledGlobal);
        return;
    }

    // 4) Pull scene-level cast and intersect (scene order wins)
    TArray<FString> SceneCast;
    if (!GetSceneCastForBlockId(SceneBlockId, SceneCast))
    {
        // If scene has no derived cast, fall back to global
        OutNames = MoveTemp(EnabledGlobal);
        return;
    }

    TSet<FString> EnabledSet;
    EnabledSet.Reserve(EnabledGlobal.Num());
    for (const TSharedPtr<FString>& N : EnabledGlobal)
    {
        if (N.IsValid())
        {
            EnabledSet.Add(*N);
        }
    }

    OutNames.Reserve(SceneCast.Num());
    for (const FString& SceneName : SceneCast)
    {
        if (EnabledSet.Contains(SceneName))
        {
            OutNames.Add(MakeShared<FString>(SceneName));
        }
    }

    // 5) If intersection is empty, fall back to global (prevents "empty dropdown" dead-end)
    if (OutNames.Num() == 0)
    {
        OutNames = MoveTemp(EnabledGlobal);
    }
}

void SGAS_ScriptTab::GetEnabledCastNames(TArray<TSharedPtr<FString>>& OutNames) const
{
    // Scene-aware filtering hook (used by Blocking spawn dropdown without UI changes)
    GetEnabledCastNames_SceneAware(OutNames);
}

// ===================================================================================
// SEQUENCER
// ===================================================================================

ULevelSequence* CreateOrLoadSceneMasterSequence(
    FGASBlock& SceneBlock,
    SGAS_ScriptTab* ScriptTab
)
{
    // If already exists → load it
    if (!SceneBlock.MasterSequencePath.IsEmpty())
    {
        ULevelSequence* Seq = LoadObject<ULevelSequence>(
            nullptr,
            *SceneBlock.MasterSequencePath
        );

        if (Seq)
        {
            if (UPackage* Pkg = Seq->GetOutermost())
            {
                Pkg->FullyLoad();
            }

            if (!IsValid(Seq))
            {
                UE_LOG(LogGASPrePro, Error, TEXT("Sequence invalid after load"));
                return nullptr;
            }

            return Seq;
        }
    }

    // Delegate to the authoritative scene sequence creator
    // This ensures naming is always SC_010, SC_020 etc
    if (!ScriptTab)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("CreateOrLoadSceneMasterSequence: ScriptTab null"));
        return nullptr;
    }

    ULevelSequence* Seq = ScriptTab->GetOrCreateSceneSequence(SceneBlock);

    if (Seq)
    {
        // Store path back onto the block so future calls load directly
        const FString FullPath = Seq->GetOutermost()->GetName();
        SceneBlock.MasterSequencePath = FullPath + TEXT(".") + Seq->GetName();
    }

    return Seq;
}


// ============================================================================
// BLOCKING FUNCTIONS
// ============================================================================

void SGAS_ScriptTab::OnStartBlockingScene(const FString& SceneId)
{
    if (!ActiveProject)
    {
        return;
    }

    // Only stop active blocking if switching scenes
    if (bBlockingActive &&
        ActiveBlockingSceneId.ToString() != SceneId)
    {
        StopBlocking();
    }

    // Only auto-open cast window for NEW blocking scenes
    bPendingAutoOpenCastWindow = false;

    FGASBlock* SceneBlock = nullptr;

    for (FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Id == SceneId && Block.Type == EGASBlockType::SceneHeading)
        {
            SceneBlock = &Block;
            break;
        }
    }

    if (!SceneBlock)
    {
        return;
    }

    // Switch to Director View immediately — before cast window or map load
    if (MainToolWindow.IsValid())
    {
        MainToolWindow.Pin()->SwitchToDirectorView(SceneId);
    }



    if (SceneHasBlockingShots(SceneId))
    {
        UE_LOG(
            LogGASPrePro,
            Warning,
            TEXT("Blocking shots already exist for scene: %s"),
            *SceneId
        );
    }


    if (!SceneBlock->BlockingLevelPath.IsEmpty())
    {

        // -----------------------------------------------------
        // Only load map if NOT already open
        // -----------------------------------------------------
        UWorld* ExistingWorld =
            GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

        const FString CurrentMap =
            ExistingWorld
            ? ExistingWorld->GetOutermost()->GetName()
            : FString();

        if (!CurrentMap.Equals(SceneBlock->BlockingLevelPath))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[BLOCKING] Loading map: %s"),
                *SceneBlock->BlockingLevelPath);

            FEditorFileUtils::LoadMap(
                SceneBlock->BlockingLevelPath,
                false,
                true
            );
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[BLOCKING] Map already open — skipping reload"));
        }

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

        if (GActiveBlockingSequence)
        {
            GActiveBlockingSequence = nullptr;
        }

        // -----------------------------------------------------
        // Existing blocking level:
        // DO NOT respawn missing cast.
        // Only refresh actors already present in the saved level.
        // -----------------------------------------------------
        if (World)
        {


            for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
            {
                AGAS_StandInActor* Actor = *It;

                if (!Actor)
                {
                    continue;
                }

                Actor->RefreshMeshFromScript(&CurrentScript);
            }
        }

        bBlockingActive = true;
        ActiveBlockingSceneId = FGuid(SceneId);

        GASBlockingAccess::SetBlockingActive(true);
        GASBlockingAccess::SetActiveSceneId(ActiveBlockingSceneId);

        RehydrateShotCamerasForScene(SceneId);
        BindToExistingShotCameras();

        // -----------------------------------------------------
        // Reopen Master Sequence
        // -----------------------------------------------------

        ULevelSequence* MasterSequence =
            LoadObject<ULevelSequence>(
                nullptr,
                *SceneBlock->MasterSequencePath
            );

        if (MasterSequence)
        {
            GActiveBlockingSequence = MasterSequence;

            // Rebuild Sequencer bindings
            if (World)
            {
                for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
                {
                    AGAS_StandInActor* Actor = *It;
                    if (!Actor)
                        continue;
                    FString CharacterName = Actor->GAS_CharacterId.ToUpper();
                    if (!SceneBlock->CharactersInScene.Contains(CharacterName))
                        continue;
                    Actor->RefreshMeshFromScript(&CurrentScript);
                    UMovieScene3DTransformTrack* TransformTrack =
                        FGASSequencerBindingUtils::EnsureActorTransformTrack(
                            MasterSequence,
                            Actor
                        );
                    UE_LOG(
                        LogGASPrePro,
                        Warning,
                        TEXT("[SEQ CHECK] %s -> TransformTrack = %s"),
                        *Actor->GetActorLabel(),
                        TransformTrack ? TEXT("VALID") : TEXT("NULL")
                    );
                }
            }

            // Open sequence AFTER level is confirmed loaded
            const FString SeqPath = MasterSequence->GetPathName();
            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([this, SeqPath](float) -> bool
                    {
                        ULevelSequence* Seq =
                            LoadObject<ULevelSequence>(nullptr, *SeqPath);
                        if (Seq && GEditor)
                        {
                            UAssetEditorSubsystem* EdSub =
                                GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
                            if (EdSub)
                                EdSub->OpenEditorForAsset(Seq);
                        }
                        return false;
                    }),
                0.5f
            );
        }

        return;
    }

    bPendingAutoOpenCastWindow = true;

    // Otherwise show set browser
    OpenSetSelectionWindow(SceneId);


}

void SGAS_ScriptTab::OpenSetSelectionWindow(const FString& SceneId)
{
    // Store pending SceneId
    PendingBlockingSceneId = SceneId;

    // Create and store window (NOT local)
    ActiveSetBrowserWindow =
        SNew(SWindow)
        .Title(FText::FromString("Select Set for Blocking"))
        .ClientSize(FVector2D(600, 500))
        .IsTopmostWindow(true)
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    ActiveSetBrowserWindow->SetContent(
        SNew(SGAS_SetBrowser)
        .OwnerScriptTab(SharedThis(this))
        .OnSetSelected(
            FOnSetSelected::CreateLambda(
                [this](const FGASSetDescriptor& Descriptor)
                {
                    AssignSetToPendingScene(Descriptor.SetId);
                }
            )
        )
    );

    FSlateApplication::Get().AddWindow(ActiveSetBrowserWindow.ToSharedRef());
}

bool SGAS_ScriptTab::CreateBlockingLevelForScene(
    FGASBlock& SceneBlock,
    const FGASSetDescriptor& SelectedSet
)
{
    if (!ActiveProject)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Blocking: ActiveProject null."));
        return false;
    }

    const FString ProjectName =
        ActiveProject->ProjectName;
    const FString SceneId = SceneBlock.Id;


    const FString FolderPath =
        TEXT("/Game/PrePro/Projects/") +
        ProjectName +
        TEXT("/Blocking/Scenes/");

    // Find scene index among scene headings
    int32 SceneIndex = 0;
    int32 Counter = 1;

    for (const FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Type == EGASBlockType::SceneHeading)
        {
            if (Block.Id == SceneId)
            {
                SceneIndex = Counter;
                break;
            }
            Counter++;
        }
    }

    // ------------------------------------------------------------
    // Resolve authoritative scene number
    // ------------------------------------------------------------

    const TMap<FString, FString> SceneNumberMap =
        FGASSceneNumberResolver::ResolveSceneNumbers(
            CurrentScript.Blocks,
            CurrentScript.SceneNumbering
        );

    const FString* FoundNumber = SceneNumberMap.Find(SceneBlock.Id);

    FString NumberPart = FoundNumber ? *FoundNumber : TEXT("0");

    // Zero-pad pure integers to 3 digits (e.g. "10" → "010"). 
    // Alphanumeric scene numbers (e.g. "12A") are left as-is.
    if (NumberPart.IsNumeric())
    {
        int32 NumVal = FCString::Atoi(*NumberPart);
        NumberPart = FString::Printf(TEXT("%03d"), NumVal);
    }


    // Sanitize heading text
    FString HeadingPart = SceneBlock.Text.ToUpper();

    HeadingPart.ReplaceInline(TEXT("."), TEXT(""));
    HeadingPart.ReplaceInline(TEXT("-"), TEXT("_"));
    HeadingPart.ReplaceInline(TEXT(" "), TEXT("_"));
    HeadingPart.ReplaceInline(TEXT("/"), TEXT("_"));
    HeadingPart.ReplaceInline(TEXT("\\"), TEXT("_"));
    HeadingPart.ReplaceInline(TEXT(":"), TEXT(""));
    HeadingPart.ReplaceInline(TEXT(","), TEXT(""));
    HeadingPart.ReplaceInline(TEXT("'"), TEXT(""));
    HeadingPart.ReplaceInline(TEXT("\""), TEXT(""));

    // Final name
    const FString MapName =
        TEXT("SC_") +
        NumberPart +
        TEXT("_") +
        HeadingPart +
        TEXT("_BLOCKING");

    const FString FullAssetPath = FolderPath + MapName;

    // Persist path immediately before deferring
    SceneBlock.BlockingLevelPath = FullAssetPath;

    UE_LOG(LogTemp, Error,
        TEXT("[BLOCKING PATH] NEW BLOCKING SCENE BRANCH"));

    MarkScriptDirty();
    EnsureScriptSaved();

    // Activate blocking state before async
    bBlockingActive = true;
    ActiveBlockingSceneId = FGuid(SceneBlock.Id);
    GASBlockingAccess::SetBlockingActive(true);
    GASBlockingAccess::SetActiveSceneId(ActiveBlockingSceneId);

    // Defer map creation by one frame to let current world fully clean up
    FTimerHandle CreateTimerHandle;
    GEditor->GetTimerManager()->SetTimer(
        CreateTimerHandle,
        FTimerDelegate::CreateLambda([this, FullAssetPath, SceneId = SceneBlock.Id, SelectedSet]()
            {
                UWorld* NewWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);

                if (!NewWorld)
                {
                    UE_LOG(LogGASPrePro, Error, TEXT("Blocking: Failed to create blank map."));
                    return;
                }

                if (!UEditorLoadingAndSavingUtils::SaveMap(NewWorld, FullAssetPath))
                {
                    UE_LOG(LogGASPrePro, Error, TEXT("Blocking: Failed to save blank map."));
                    return;
                }

                UE_LOG(LogTemp, Error,
                    TEXT("[BLOCKING SAVE] Scene=%s SavedPath=%s"),
                    *SceneId,
                    *FullAssetPath);

                UEditorLoadingAndSavingUtils::LoadMap(FullAssetPath);

                FTimerHandle InitTimerHandle;
                GEditor->GetTimerManager()->SetTimer(
                    InitTimerHandle,
                    FTimerDelegate::CreateLambda([this, SelectedSet]()
                        {
                            UWorld* World = GEditor->GetEditorWorldContext().World();
                            if (!World) return;

                            FGASStageLightingUtils::SetupDefaultStageLighting(World);
                            UE_LOG(LogGASPrePro, Warning, TEXT("Blocking: Lighting initialized."));
                            FinalizeBlockingLevel(World);

                            // Duplicate set into blocking level
                            if (!DuplicateSetIntoBlockingLevel(SelectedSet.LevelPath, World))
                            {
                                UE_LOG(LogGASPrePro, Error, TEXT("Blocking: Set duplication failed"));
                            }

                            // Spawn cast
                            FGASBlock* SceneBlockPtr = GetSceneBlockById(
                                ActiveBlockingSceneId.ToString()
                            );
                            if (SceneBlockPtr)
                            {
                                SpawnSceneCast(SceneBlockPtr);
                            }

                            // Save
                            FString LoadedMapName = World->GetOutermost()->GetName();
                            if (!UEditorLoadingAndSavingUtils::SaveMap(World, LoadedMapName))
                            {
                                UE_LOG(LogGASPrePro, Error, TEXT("Blocking: FAILED TO SAVE AFTER INIT"));
                            }
                            else
                            {
                                UE_LOG(LogGASPrePro, Warning, TEXT("Blocking: Saved AFTER initialization"));
                            }
                        }),
                    0.1f,
                    false
                );
            }),
        0.05f,
        false
    );

    return true;
}

bool SGAS_ScriptTab::CreateEmptySceneLevel(
    FGASBlock& SceneBlock
)
{
    if (!ActiveProject)
    {
        UE_LOG(LogGASPrePro, Error,
            TEXT("[SceneLevel] ActiveProject NULL"));

        return false;
    }

    const FString ProjectName =
        ActiveProject->ProjectName;

    const FString SceneName =
        SceneBlock.Text
        .Replace(TEXT(" "), TEXT("_"))
        .Replace(TEXT("."), TEXT(""))
        .Replace(TEXT(":"), TEXT(""))
        .Replace(TEXT("/"), TEXT("_"));

    const FString MapName =
        FString::Printf(
            TEXT("%s_SCENE"),
            *SceneName
        );

    const FString PackagePath =
        FString::Printf(
            TEXT("/Game/PrePro/Projects/%s/Blocking/Scenes/%s"),
            *ProjectName,
            *MapName
        );

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[SceneLevel] Creating Empty Scene Level: %s"),
        *PackagePath);

    UPackage* Package =
        CreatePackage(*PackagePath);

    if (!Package)
    {
        UE_LOG(LogGASPrePro, Error,
            TEXT("[SceneLevel] Failed to create package"));

        return false;
    }

    UWorldFactory* WorldFactory =
        NewObject<UWorldFactory>();

    if (!WorldFactory)
    {
        UE_LOG(LogGASPrePro, Error,
            TEXT("[SceneLevel] Failed to create world factory"));

        return false;
    }

    const FString AssetName =
        FPackageName::GetLongPackageAssetName(PackagePath);

    UWorld* NewWorld =
        Cast<UWorld>(
            WorldFactory->FactoryCreateNew(
                UWorld::StaticClass(),
                Package,
                *AssetName,
                RF_Public | RF_Standalone,
                nullptr,
                GWarn
            )
        );

    if (!NewWorld)
    {
        UE_LOG(LogGASPrePro, Error,
            TEXT("[SceneLevel] Failed to create world asset"));

        return false;
    }

    FAssetRegistryModule::AssetCreated(NewWorld);

    Package->MarkPackageDirty();

    const FString FilePath =
        FPackageName::LongPackageNameToFilename(
            PackagePath,
            FPackageName::GetMapPackageExtension()
        );

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

    const bool bSaved =
        UPackage::SavePackage(
            Package,
            NewWorld,
            *FilePath,
            SaveArgs
        );

    if (!bSaved)
    {
        UE_LOG(LogGASPrePro, Error,
            TEXT("[SceneLevel] Failed to save world"));

        return false;
    }

    SceneBlock.BlockingLevelPath = PackagePath;

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[SceneLevel] Empty scene level created successfully"));

    return true;
}

void SGAS_ScriptTab::SpawnSelectedSet(const FString& LevelPath)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("SpawnSelectedSet: No world"));
        return;
    }

    UE_LOG(LogGASPrePro, Warning, TEXT("Streaming level: %s"), *LevelPath);

    bool bSuccess = false;

    ULevelStreamingDynamic* StreamingLevel =
        ULevelStreamingDynamic::LoadLevelInstance(
            World,
            LevelPath,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            bSuccess
        );

    if (!bSuccess || !StreamingLevel)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Failed to stream level: %s"), *LevelPath);
    }
}

void SGAS_ScriptTab::OpenCastWindowForScene(const FString& SceneId)
{

    // -----------------------------------------------------
    // If window already exists → bring to front (DO NOT destroy)
    // -----------------------------------------------------
    if (BlockingCastWindow.IsValid())
    {
        if (TSharedPtr<SWindow> ExistingWindow = BlockingCastWindow.Pin())
        {
            FSlateApplication::Get().SetKeyboardFocus(ExistingWindow);
            return;
        }
        BlockingCastWindow.Reset();
    }

    // -----------------------------------------------------
    // Resolve Scene Title
    // -----------------------------------------------------
    FString SceneTitle = TEXT("Unknown Scene");

    for (const FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Id == SceneId)
        {
            SceneTitle = Block.Text;
            break;
        }
    }

    const FString WindowTitle =
        FString::Printf(TEXT("CAST | %s"), *SceneTitle.ToUpper());

    TSharedRef<SWindow> Window =
        SNew(SWindow)
        .Title(FText::FromString(WindowTitle))
        .ClientSize(FVector2D(600.f, 400.f))
        .SizingRule(ESizingRule::UserSized)
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    Window->SetContent(
        SNew(SGAS_BlockingCastWindow)
        .SceneId(FGuid(SceneId))
        .Script(&CurrentScript)
        .BlockingSequence(GActiveBlockingSequence)
        .OnCastModified(
            SGAS_BlockingCastWindow::FOnCastModified::CreateSP(
                this,
                &SGAS_ScriptTab::OnBlockingCastModified
            )
        )
    );

    BlockingCastWindow = Window;

    Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
        [this](const TSharedRef<SWindow>&)
        {
            BlockingCastWindow.Reset();
        }
    ));

    // -----------------------------------------------------
    // Attach to editor window (guarantees front)
    // -----------------------------------------------------
    TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();

    if (ParentWindow.IsValid())
    {
        FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow.ToSharedRef());
    }
    else
    {
        FSlateApplication::Get().AddWindow(Window);
    }

    // -----------------------------------------------------
    // Force focus (critical)
    // -----------------------------------------------------
    FSlateApplication::Get().SetKeyboardFocus(Window);
}

void SGAS_ScriptTab::OnBlockingCastModified()
{
    UE_LOG(LogGASPrePro, Warning, TEXT("Blocking cast modified — saving"));
    bIsUpdatingCast = true;
    MarkScriptDirty();
    SaveScriptToJson();

    if (GEditor)
    {
        if (UWorld* World = GEditor->GetEditorWorldContext().World())
        {
            World->MarkPackageDirty();
        }
    }

    if (GActiveBlockingSequence)
    {
        GActiveBlockingSequence->MarkPackageDirty();
    }

    bIsUpdatingCast = false;

    // Refresh Director View cast list
    if (MainToolWindow.IsValid())
    {
        MainToolWindow.Pin()->RefreshDirectorViewCast();
    }
}

void SGAS_ScriptTab::SpawnSceneCast(FGASBlock* SceneBlock)
{

    UE_LOG(LogTemp, Error, TEXT("🔥 SpawnSceneCast CALLED"));
    if (!SceneBlock)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return;
    }

    // Sequencer / LevelSequenceActor may not exist yet when cast spawns.
    // Binding is handled with a delayed retry per-actor after spawn.

    // Spawn into persistent level explicitly
    ULevel* TargetLevel = World->PersistentLevel;
    if (!TargetLevel)
    {
        return;
    }

    float XOffset = 0.f;

    for (const FString& CharacterName : SceneBlock->CharactersInScene)
    {
        FString UpperName = CharacterName.ToUpper();

        bool bAlreadyExists = false;

        for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
        {
            if (!*It) continue;

            if (It->GAS_CharacterId == UpperName)
            {
                bAlreadyExists = true;
                break;
            }
        }

        // 🔥 CRITICAL FIX: prevent respawn of removed actors
        if (!SceneBlock->CharactersInScene.Contains(CharacterName))
        {
            UE_LOG(LogTemp, Warning, TEXT("SKIP SPAWN (removed): %s"), *CharacterName);
            continue;
        }

        if (bAlreadyExists)
        {
            UE_LOG(LogTemp, Warning, TEXT("SKIP SPAWN (already exists): %s"), *CharacterName);
            continue;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.OverrideLevel = TargetLevel;
        SpawnParams.Name = MakeUniqueObjectName(
            TargetLevel,
            AGAS_StandInActor::StaticClass(),
            FName(*UpperName)
        );
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AGAS_StandInActor* StandIn =
            World->SpawnActor<AGAS_StandInActor>(
                AGAS_StandInActor::StaticClass(),
                FVector(XOffset, 0.f, 0.f),
                FRotator::ZeroRotator,
                SpawnParams
            );

        if (StandIn)
        {
            StandIn->GAS_CharacterId = UpperName;
            StandIn->SetActorLabel(UpperName);

            // --------------------------------------------------
            // Apply script default mesh
            // --------------------------------------------------

            FGASCharacterDefinition* Def =
                CurrentScript.FindCharacterDefinition(UpperName);

            UE_LOG(LogTemp, Error, TEXT("CHAR LOOKUP → %s"), *UpperName);

            if (!Def)
            {
                UE_LOG(LogTemp, Error, TEXT("❌ NO CHARACTER DEF FOUND"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("DEF FOUND → MeshPath: %s"), *Def->DefaultMeshPath);
            }

            if (Def && !Def->DefaultMeshPath.IsEmpty())
            {
                USkeletalMesh* Mesh =
                    LoadObject<USkeletalMesh>(nullptr, *Def->DefaultMeshPath);

                if (Mesh)
                {
                    StandIn->SetCharacterSkeletalMesh(Mesh);
                }
            }

            StandIn->SetFlags(RF_Transactional);
            StandIn->SetIsTemporarilyHiddenInEditor(false);
            StandIn->SetActorEnableCollision(true);
            StandIn->SetActorHiddenInGame(false);

            // Bind to sequence if available
            if (GActiveBlockingSequence)
            {
                FGASSequencerBindingUtils::EnsureActorTransformTrack(
                    GActiveBlockingSequence,
                    StandIn
                );
            }
        }

        XOffset += 150.f;
    }
}

void SGAS_ScriptTab::LoadSetForBlocking(const FName& SetId)
{
    if (!GEditor)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return;
    }

    // ------------------------------------------------------------
    // 1. Remove existing streamed GAS set levels
    // ------------------------------------------------------------
    TArray<ULevelStreaming*> LevelsToRemove;

    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (!StreamingLevel)
        {
            continue;
        }

        const FString PackageName = StreamingLevel->GetWorldAssetPackageName();

        if (PackageName.Contains(TEXT("/Game/PrePro/Blocking/Sets/")))
        {
            LevelsToRemove.Add(StreamingLevel);
        }
    }

    for (ULevelStreaming* Level : LevelsToRemove)
    {
        World->RemoveStreamingLevel(Level);
    }

    // ------------------------------------------------------------
    // 2. Find set descriptor
    // ------------------------------------------------------------
    const TArray<FGASSetDescriptor>& Sets =
        FGASSetDiscovery::GetAvailableSets();

    FString LevelPath;

    for (const FGASSetDescriptor& Set : Sets)
    {
        if (Set.SetId == SetId)
        {
            LevelPath = Set.LevelPath;
            break;
        }
    }

    if (LevelPath.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Set level not found for %s"), *SetId.ToString());
        return;
    }

    // ------------------------------------------------------------
    // 3. Load streaming level
    // ------------------------------------------------------------
    bool bSuccess = false;

    ULevelStreamingDynamic* StreamingLevel =
        ULevelStreamingDynamic::LoadLevelInstance(
            World,
            LevelPath,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            bSuccess
        );

    if (!StreamingLevel || !bSuccess)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Failed to stream set level: %s"), *LevelPath);
        return;
    }


    if (!StreamingLevel)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Failed to stream set level: %s"), *LevelPath);
        return;
    }

    StreamingLevel->SetShouldBeVisible(true);
    StreamingLevel->SetShouldBeLoaded(true);

    UE_LOG(LogGASPrePro, Warning, TEXT("Loaded set level: %s"), *LevelPath);
}

void SGAS_ScriptTab::StopBlocking()
{
    if (!bBlockingActive)
        return;

    ResetShotModeState();

    UE_LOG(LogGASPrePro, Warning, TEXT("[BLOCKING] Stopping blocking mode"));

    BoundShotCameraSet.Empty();

    GASBlockingAccess::SetBlockingActive(false);

    bBlockingActive = false;
    ActiveBlockingSceneId.Invalidate();
}

void SGAS_ScriptTab::OnDeleteBlockingScene(const FString& SceneId)
{
    FGASBlock* SceneBlock = nullptr;

    for (FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Id == SceneId &&
            Block.Type == EGASBlockType::SceneHeading)
        {
            SceneBlock = &Block;
            break;
        }
    }

    if (!SceneBlock || SceneBlock->BlockingLevelPath.IsEmpty())
    {
        return;
    }

    const FString BlockingPath = SceneBlock->BlockingLevelPath;

    const EAppReturnType::Type Result =
        FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::FromString(TEXT("Delete blocking level for this scene?"))
        );

    if (Result != EAppReturnType::Yes)
    {
        return;
    }

    UObject* Asset = LoadObject<UObject>(nullptr, *BlockingPath);

    if (!Asset)
    {
        return;
    }

    TArray<UObject*> AssetsToDelete;
    AssetsToDelete.Add(Asset);

    const int32 NumDeleted = ObjectTools::DeleteObjects(AssetsToDelete);

    // Only clear if deletion actually happened
    if (NumDeleted > 0)
    {
        SceneBlock->BlockingLevelPath.Empty();
        SceneBlock->AssignedSetId = NAME_None; // optional if you want set cleared too

        RebuildShotList();
    }

    // If deleting currently active blocking scene, clear state
    if (ActiveBlockingSceneId.ToString() == SceneId)
    {
        ActiveBlockingSceneId.Invalidate();
        bBlockingActive = false;
    }

}

void SGAS_ScriptTab::ResumePendingBlocking()
{
    if (PendingBlockingSceneId.IsEmpty())
    {
        return;
    }

    FString SceneId = PendingBlockingSceneId;
    PendingBlockingSceneId.Empty();

    PromoteNonBlockingShotsToBlocking(SceneId);

    OnStartBlockingScene(SceneId);
}

void SGAS_ScriptTab::AssignSetToPendingScene(FName SelectedSetId)
{
    StopBlocking();
    const FString SceneId = PendingBlockingSceneId;

    if (SceneId.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("AssignSetToPendingScene: No pending SceneId"));
        return;
    }

    // Resolve SceneBlock from authoritative script
    FGASBlock* SceneBlock = nullptr;

    for (FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Id == SceneId)
        {
            SceneBlock = &Block;
            break;
        }
    }

    if (!SceneBlock)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("AssignSetToPendingScene: SceneBlock not found."));
        return;
    }

    // Resolve set descriptor
    const TArray<FGASSetDescriptor>& Sets = FGASSetDiscovery::GetAvailableSets();

    const FGASSetDescriptor* Found = nullptr;
    for (const FGASSetDescriptor& Set : Sets)
    {
        if (Set.SetId == SelectedSetId)
        {
            Found = &Set;
            break;
        }
    }

    if (!Found)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("AssignSetToPendingScene: Set not found: %s"), *SelectedSetId.ToString());
        return;
    }

    // Create blocking level and assign path
    if (!CreateBlockingLevelForScene(*SceneBlock, *Found))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("AssignSetToPendingScene: Failed to create blocking level."));
        return;
    }

    // Store the assigned set ID on the block
    SceneBlock->AssignedSetId = SelectedSetId;

    // Persist script change
    MarkScriptDirty();
    SaveScriptToJson();

    // -----------------------------------------------------
    // Ensure Scene Master Sequence (Rehearsal Timeline)
    // -----------------------------------------------------
    FString SequenceName = SceneBlock->BlockingLevelPath;

    // Extract map name from path
    SequenceName = FPackageName::GetShortName(SequenceName);

    // Replace suffix
    SequenceName = SequenceName.Replace(TEXT("_BLOCKING"), TEXT("_Master"));

    ULevelSequence* MasterSequence =
        CreateOrLoadSceneMasterSequence(*SceneBlock, this);

    if (!MasterSequence)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("AssignSetToPendingScene: MasterSequence null"));
        return;
    }

    // 🔥 FORCE FULL PACKAGE LOAD
    if (UPackage* SeqPkg = MasterSequence->GetOutermost())
    {
        SeqPkg->FullyLoad();
    }

    GActiveBlockingSequence = MasterSequence;

    UAssetEditorSubsystem* AssetEditorSubsystem =
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

    if (AssetEditorSubsystem)
    {
        AssetEditorSubsystem->OpenEditorForAsset(MasterSequence);

        UE_LOG(LogGASPrePro, Warning, TEXT("[SEQ] Opening MasterSequence: %s"),
            *MasterSequence->GetName());

        // Delay binding so Sequencer is fully initialized
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

        if (World && MasterSequence)
        {
            UE_LOG(LogGASPrePro, Warning, TEXT("[SEQ] Initial bind stand-ins (FILTERED)"));

            if (SceneBlock)
            {
                for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
                {
                    AGAS_StandInActor* Actor = *It;
                    if (!Actor) continue;

                    FString ActorId = Actor->GAS_CharacterId.TrimStartAndEnd();

                    // ✅ MUST exist in scene
                    if (!SceneBlock->CharactersInScene.Contains(ActorId))
                    {
                        continue;
                    }

                    // ✅ NEW: prevent duplicate/rebind
                    if (FGASSequencerBindingUtils::IsActorAlreadyBound(MasterSequence, Actor))
                    {
                        UE_LOG(LogTemp, Warning, TEXT("SKIP already bound: %s"), *ActorId);
                        continue;
                    }

                    FGASSequencerBindingUtils::EnsureActorTransformTrack(
                        MasterSequence,
                        Actor
                    );
                }
            }
        }
    }

    SaveSequenceAsset(MasterSequence);

    // Close window
    if (ActiveSetBrowserWindow.IsValid())
    {
        ActiveSetBrowserWindow->RequestDestroyWindow();
        ActiveSetBrowserWindow.Reset();
    }

    bLoadingBlockingLevel = true;
    PendingBlockingSceneId.Empty();
}

static void MuteNonBlockingTracksForEditing(ULevelSequence* Seq, UWorld* World)
{
    if (!IsValid(Seq) || !World) return;

    UMovieScene* MovieScene = Seq->GetMovieScene();
    if (!MovieScene) return;

    // Mute Camera Cuts track
    UMovieSceneTrack* CameraCutTrack = MovieScene->GetCameraCutTrack();
    if (CameraCutTrack)
    {
        CameraCutTrack->SetEvalDisabled(true);
    }

    // Remove any possessable binding that isn't a StandIn actor
    TArray<FGuid> BindingsToRemove;

    for (int32 i = 0; i < MovieScene->GetPossessableCount(); ++i)
    {
        const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);

        const UClass* PossessableClass = Possessable.GetPossessedObjectClass();

        if (!PossessableClass)
            continue;

        if (!PossessableClass || !PossessableClass->IsChildOf(AGAS_StandInActor::StaticClass()))
        {
            BindingsToRemove.Add(Possessable.GetGuid());
        }
    }

    for (const FGuid& Guid : BindingsToRemove)
    {
        Seq->UnbindPossessableObjects(Guid);
        MovieScene->RemovePossessable(Guid);
    }

    Seq->MarkPackageDirty();
}

void SGAS_ScriptTab::FinalizeBlockingLevel(UWorld* World)
{
    if (!World)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("FinalizeBlockingLevel: World null"));
        return;
    }

    FString LoadedMapName = World->GetOutermost()->GetName();

    if (!UEditorLoadingAndSavingUtils::SaveMap(World, LoadedMapName))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Blocking: FAILED TO SAVE AFTER INIT"));
    }
    else
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("Blocking: Saved AFTER initialization"));
    }

    // Find the scene sequence via the active blocking scene
    FGASBlock* SceneBlock = nullptr;
    for (FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Id == ActiveBlockingSceneId.ToString())
        {
            SceneBlock = &Block;
            break;
        }
    }

    if (!SceneBlock || SceneBlock->MasterSequencePath.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("[BLOCKING] No sequence path on SceneBlock"));
        return;
    }

    ULevelSequence* Seq = LoadObject<ULevelSequence>(
        nullptr, *SceneBlock->MasterSequencePath);

    if (!IsValid(Seq))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("[BLOCKING] Failed to load scene sequence: %s"),
            *SceneBlock->MasterSequencePath);
        return;
    }

    GActiveBlockingSequence = Seq;

    UAssetEditorSubsystem* AssetEditorSubsystem =
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

    if (AssetEditorSubsystem)
    {
        AssetEditorSubsystem->OpenEditorForAsset(Seq);
    }

    MuteNonBlockingTracksForEditing(Seq, World);

    UE_LOG(LogGASPrePro, Warning, TEXT("[BLOCKING] Opened scene sequence: %s"),
        *Seq->GetName());
}


// ============================================================================
// HELPER FUNCTIONS - Numbering style
// ============================================================================

void SGAS_ScriptTab::ApplySceneNumberingBaseStyle(EGASSceneNumberBaseStyle InBaseStyle)
{
    // Update authoritative model
    CurrentScript.SceneNumbering.BaseStyle = InBaseStyle;

    // Mark dirty so JSON stays in sync
    MarkScriptDirty();

    // Rebuild dependent UI
    RebuildShotList();

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->Invalidate(EInvalidateWidget::LayoutAndVolatility);
    }

}


// ============================================================================
// PARAGRAPH CLICKED — shot marking logic
// ============================================================================
void SGAS_ScriptTab::OnScriptParagraphClicked(int32 BlockIndex)
{
    UE_LOG(LogGASPrePro, Error, TEXT("SCRIPT TAB: Paragraph clicked index=%d"), BlockIndex);

    // Make sure index is valid
    if (!CurrentScript.Blocks.IsValidIndex(BlockIndex))
        return;



    // -------------------------------------------------
    // EDIT MODE (THIS WAS MISSING)
    // -------------------------------------------------
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->TryEditParagraph(BlockIndex);
    }
}


FReply SGAS_ScriptTab::OnClearScriptClicked()
{
    ResetShotModeState();

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->ResetEditorModes();
    }

    ClearScript();
    return FReply::Handled();
}


static FString ResolveShotLabel_Stub(
    int32 InSceneIndex,
    int32 InShotIndex,
    EGASShotNumberingPolicy InPolicy
)
{
    // STUB ONLY — numbering logic implemented later
    return FString();
}

// ============================================================================
// Build Shot List UI
// ============================================================================
void SGAS_ScriptTab::RebuildShotList()
{

    // --------------------------------------------------
    // V2 SCENE BUILD (authoritative)
    // --------------------------------------------------
    TArray<FGASShotListSceneRow> SceneRowsV2;
    if (ScriptPanel.IsValid())
    {
        const TArray<FRenderedParagraph>& Rendered =
            ScriptPanel->GetRenderedParagraphs();

        if (Rendered.Num() == 0)
        {
            UE_LOG(LogGASPrePro, Verbose, TEXT("[ShotListV2] RenderedParagraphs is empty"));
        }
        else
        {
            BuildShotListFromScriptV2(
                CurrentScript,
                CurrentScript.SceneNumbering,
                Rendered,
                SceneRowsV2
            );


            // --------------------------------------------------
            // Normalize Scene Heading + Time (Display Only)
            // --------------------------------------------------
            for (FGASShotListSceneRow& Scene : SceneRowsV2)
            {
                if (!Scene.TimeOfDayOverride.IsEmpty())
                {
                    continue;
                }

                FString ParsedTOD;
                FString DisplayHeading = Scene.SceneHeading;

                GAS_SplitHeading_TimeOfDay(DisplayHeading, ParsedTOD);

                if (!ParsedTOD.IsEmpty() && DisplayHeading != Scene.SceneHeading)
                {
                    Scene.SceneHeading = DisplayHeading;
                    Scene.TimeOfDayOverride = ParsedTOD;
                }
            }

            // --------------------------------------------------
            // Copy Assigned Set from Script Blocks into V2 rows
            // --------------------------------------------------
            for (FGASShotListSceneRow& Scene : SceneRowsV2)
            {
                for (const FGASBlock& Block : CurrentScript.Blocks)
                {
                    if (Block.Id == Scene.SceneId &&
                        Block.Type == EGASBlockType::SceneHeading)
                    {
                        Scene.SetId = Block.AssignedSetId.IsNone()
                            ? FString()
                            : Block.AssignedSetId.ToString();

                        break;
                    }
                }
            }


        }
    }

#if 0 // LEGACY SHOT LIST PIPELINE (disabled during V2 swap)


    // -------------------------------------------------
    // Helper: find owning SceneBlockIndex for ANY block
    // (walk backward to nearest SceneHeading)
    // -------------------------------------------------
    auto FindOwningSceneBlockIndex =
        [this](const FString& BlockId) -> int32
        {
            if (BlockId.IsEmpty())
            {
                return INDEX_NONE;
            }

            int32 BlockIndex = INDEX_NONE;

            for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
            {
                if (CurrentScript.Blocks[i].Id == BlockId)
                {
                    BlockIndex = i;
                    break;
                }
            }

            if (BlockIndex == INDEX_NONE)
            {
                return INDEX_NONE;
            }

            for (int32 i = BlockIndex; i >= 0; --i)
            {
                if (CurrentScript.Blocks[i].Type == EGASBlockType::SceneHeading)
                {
                    return i;
                }
            }

            return INDEX_NONE;
        };


    // -------------------------------------------------
    // STEP 5 (PART 4): ONE-PASS SHOT BUILD
    // Derived first, User second, scene-local numbering
    // -------------------------------------------------

    TMap<FString, TArray<const FGASMarker*>> UserByScene;

    for (const FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        UserByScene.FindOrAdd(Marker.BlockId).Add(&Marker);

    }

    // Walk V2 scenes in authoritative order for shot attachment
    for (const FGASShotListSceneRow& Scene : SceneRowsV2)
    {
        const FString& SceneBlockId = Scene.SceneId;

        // Find legacy SceneBlockIndex from SceneId
        int32 SceneBlockIndex = INDEX_NONE;
        for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
        {
            if (CurrentScript.Blocks[i].Id == SceneBlockId)
            {
                SceneBlockIndex = i;
                break;
            }
        }

        if (!CurrentScript.Blocks.IsValidIndex(SceneBlockIndex))
        {
            continue;
        }

        const TArray<const FGASMarker*>* UserShots =
            UserByScene.Find(SceneBlockId);

        int32 ShotIndexZeroBased = 0;

        if (UserShots)
        {
            for (const FGASMarker* Marker : *UserShots)
            {
                FGASShotDefinitionListRow ShotRow;
                ShotRow.bIsShotRow = true;
                ShotRow.SceneBlockIndex = SceneBlockIndex;
                ShotRow.SceneId = SceneBlockId;
                ShotRow.MarkerId = Marker->Id;

                ShotRow.DisplayName =
                    ResolveShotDisplayLabel(
                        CurrentScript,
                        SceneBlockIndex,
                        ShotIndexZeroBased++
                    );

                ShotListItems.Add(
                    MakeShared<FGASShotDefinitionListRow>(ShotRow)
                );
            }
        }
    }





    // -------------------------------------------------
    // Reorder ShotListItems so shots follow their parent scene deterministically
    // -------------------------------------------------
    {
        TArray<TSharedPtr<FGASShotDefinitionListRow>> OrderedItems;

        // Cache shot rows grouped by SceneBlockIndex
        TMultiMap<FString, TSharedPtr<FGASShotDefinitionListRow>> ShotsByScene;

        for (const TSharedPtr<FGASShotDefinitionListRow>& Row : ShotListItems)
        {
            if (Row.IsValid() && Row->bIsShotRow)
            {
                if (CurrentScript.Blocks.IsValidIndex(Row->SceneBlockIndex))
                {
                    const FString& SceneBlockId =
                        CurrentScript.Blocks[Row->SceneBlockIndex].Id;

                    ShotsByScene.Add(SceneBlockId, Row);
                }

            }
        }

        // Walk scenes in original order and append their shots
        for (const TSharedPtr<FGASShotDefinitionListRow>& Row : ShotListItems)
        {
            if (!Row.IsValid() || Row->bIsShotRow)
            {
                continue;
            }

            // Scene row first
            OrderedItems.Add(Row);

            // Then its shots — ONLY if scene is expanded
            const int32 SceneBlockIndex = Row->SceneBlockIndex;

            if (CurrentScript.Blocks.IsValidIndex(SceneBlockIndex))
            {
                const FString& SceneBlockId =
                    CurrentScript.Blocks[SceneBlockIndex].Id;

                if (ExpandedScenes.Contains(SceneBlockId))
                {
                    TArray<TSharedPtr<FGASShotDefinitionListRow>> SceneShots;
                    ShotsByScene.MultiFind(SceneBlockId, SceneShots);

                    for (const TSharedPtr<FGASShotDefinitionListRow>& ShotRow : SceneShots)
                    {
                        OrderedItems.Add(ShotRow);
                    }
                }
            }

        }

        ShotListItems = MoveTemp(OrderedItems);
    }

#endif // LEGACY SHOT LIST PIPELINE

    // --------------------------------------------------
    // BUILD SCENE LIST UI (RIGHT PANEL)
    // --------------------------------------------------
    if (!ShotListContainer.IsValid())
    {
        return;
    }

    UE_LOG(
        LogGASPrePro,
        Verbose,
        TEXT("[ShotList UI] Children BEFORE clear = %d"),
        ShotListContainer->GetChildren()->Num()
    );


    ShotListContainer->ClearChildren();

    // =====================================================
    // COLUMN WIDTHS
    // =====================================================
    const float CW_Expand = 24.f;
    const float CW_Page = 30.f;
    const float CW_SceneNum = 50.f;
    const float CW_Heading = 280.f;
    const float CW_Eighths = 45.f;
    const float CW_Time = 55.f;
    const float CW_Set = 130.f;
    const float CW_Notes = 180.f;

    const float CW_Lock = 24.f;
    const float CW_Shot = 45.f;
    const float CW_Type = 45.f;
    const float CW_Move = 36.f;
    const float CW_SEighths = 40.f;
    const float CW_Desc = 220.f;
    const float CW_Lens = 45.f;
    const float CW_Camera = 80.f;
    const float CW_SNotes = 400.f;

    // =====================================================
    // HEADER ROW
    // =====================================================
    auto MakeHeaderCell = [](const FString& Label, float Width) -> TSharedRef<SWidget>
        {
            return SNew(SBox)
                .WidthOverride(Width)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Label))
                        .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                ];
        };

    ShotListContainer->AddSlot()
        .AutoHeight()
        .Padding(2.f, 4.f)
        [
            SNew(SBorder)
                .Padding(4.f)
                .BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()[MakeHeaderCell(TEXT(""), CW_Expand)]
                        + SHorizontalBox::Slot().AutoWidth()[MakeHeaderCell(TEXT("PG"), CW_Page)]
                        + SHorizontalBox::Slot().AutoWidth()[MakeHeaderCell(TEXT("SC#"), CW_SceneNum)]
                        + SHorizontalBox::Slot().AutoWidth()[MakeHeaderCell(TEXT("Scene Heading"), CW_Heading)]
                        + SHorizontalBox::Slot().AutoWidth()[MakeHeaderCell(TEXT("1/8"), CW_Eighths)]
                        + SHorizontalBox::Slot().AutoWidth()[MakeHeaderCell(TEXT("Time"), CW_Time)]
                        + SHorizontalBox::Slot().AutoWidth()[MakeHeaderCell(TEXT("Set"), CW_Set)]
                        + SHorizontalBox::Slot().AutoWidth()[MakeHeaderCell(TEXT("Notes"), CW_Notes)]
                ]
        ];

    if (SceneRowsV2.Num() == 0)
    {
        UE_LOG(LogGASPrePro, Verbose, TEXT("[ShotListV2] No scenes to render"));
        return;
    }

    for (const FGASShotListSceneRow& Scene : SceneRowsV2)
    {
        const FString CapturedSceneId = Scene.SceneId;
        const int32 CapturedShotCount = Scene.Shots.Num();

        // =====================================================
        // SCENE ROW
        // =====================================================
        ShotListContainer->AddSlot()
            .AutoHeight()
            .Padding(2.f, 1.f)
            [
                SNew(SHorizontalBox)

                    // Expand arrow
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SBox)
                            .WidthOverride(CW_Expand)
                            [
                                SNew(SButton)
                                    .ButtonStyle(FAppStyle::Get(), "NoBorder")
                                    .OnClicked_Lambda([this, Scene]()
                                        {
                                            if (ExpandedScenes.Contains(Scene.SceneId))
                                                ExpandedScenes.Remove(Scene.SceneId);
                                            else
                                                ExpandedScenes.Add(Scene.SceneId);
                                            RebuildShotList();
                                            return FReply::Handled();
                                        })
                                    [
                                        SNew(STextBlock)
                                            .Text(ExpandedScenes.Contains(Scene.SceneId)
                                                ? FText::FromString(TEXT("▼"))
                                                : FText::FromString(TEXT("▶")))
                                    ]
                            ]
                    ]

                // PG
                + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(CW_Page)
                            [
                                SNew(STextBlock)
                                    .Text(FText::AsNumber(Scene.StartPage))
                            ]
                    ]

                // SC# + status dot
                + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(CW_SceneNum)
                            [
                                SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(0.f, 0.f, 4.f, 0.f)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("●")))
                                            .ColorAndOpacity_Lambda([this, SceneId = Scene.SceneId, ShotCount = Scene.Shots.Num()]()
                                                {
                                                    for (const FGASBlock& Block : CurrentScript.Blocks)
                                                    {
                                                        if (Block.Id == SceneId)
                                                        {
                                                            if (!Block.bSequenceBuilt)
                                                                return FSlateColor(FLinearColor(1.f, 0.2f, 0.2f, 1.f));
                                                            if (Block.SequenceBuiltShotCount == ShotCount)
                                                                return FSlateColor(FLinearColor(0.2f, 1.f, 0.2f, 1.f));
                                                            return FSlateColor(FLinearColor(1.f, 0.85f, 0.1f, 1.f));
                                                        }
                                                    }
                                                    return FSlateColor(FLinearColor(1.f, 0.2f, 0.2f, 1.f));
                                                })
                                            .ToolTipText_Lambda([this, SceneId = Scene.SceneId, ShotCount = Scene.Shots.Num()]()
                                                {
                                                    for (const FGASBlock& Block : CurrentScript.Blocks)
                                                    {
                                                        if (Block.Id == SceneId)
                                                        {
                                                            if (!Block.bSequenceBuilt)
                                                                return FText::FromString(TEXT("Sequence not built"));
                                                            if (Block.SequenceBuiltShotCount == ShotCount)
                                                                return FText::FromString(TEXT("Sequence built and up to date"));
                                                            return FText::FromString(FString::Printf(
                                                                TEXT("Sequence built with %d shots — current count is %d"),
                                                                Block.SequenceBuiltShotCount, ShotCount));
                                                        }
                                                    }
                                                    return FText::FromString(TEXT("Sequence not built"));
                                                })
                                    ]
                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(Scene.SceneNumber))
                                    ]
                            ]
                    ]

                // Scene Heading (clickable)
                + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(CW_Heading)
                            [
                                SNew(SBorder)
                                    .VAlign(VAlign_Center)
                                    .OnMouseButtonDown_Lambda(
                                        [this, Scene, CapturedSceneId](const FGeometry&, const FPointerEvent& MouseEvent)
                                        {
                                            if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
                                            {
                                                FMenuBuilder MenuBuilder(true, nullptr);

                                                FString BlockingPath;
                                                for (const FGASBlock& Block : CurrentScript.Blocks)
                                                {
                                                    if (Block.Id == Scene.SceneId && Block.Type == EGASBlockType::SceneHeading)
                                                    {
                                                        BlockingPath = Block.BlockingLevelPath;
                                                        break;
                                                    }
                                                }

                                                const bool bHasBlocking = !BlockingPath.IsEmpty();

                                                if (!bHasBlocking)
                                                {
                                                    MenuBuilder.AddMenuEntry(
                                                        FText::FromString(TEXT("Start Blocking")),
                                                        FText::FromString(TEXT("Create blocking level for this scene")),
                                                        FSlateIcon(),
                                                        FUIAction(FExecuteAction::CreateLambda(
                                                            [this, SceneId = CapturedSceneId]()
                                                            {
                                                                FSlateApplication::Get().DismissAllMenus();
                                                                TWeakPtr<SGAS_ScriptTab> WeakThis = SharedThis(this);
                                                                FTSTicker::GetCoreTicker().AddTicker(
                                                                    FTickerDelegate::CreateLambda([WeakThis, SceneId](float)
                                                                        {
                                                                            if (!WeakThis.IsValid()) return false;
                                                                            WeakThis.Pin()->PromoteNonBlockingShotsToBlocking(SceneId);
                                                                            WeakThis.Pin()->OnStartBlockingScene(SceneId);
                                                                            return false;
                                                                        })
                                                                );
                                                            }
                                                        ))
                                                    );
                                                }
                                                else
                                                {
                                                    MenuBuilder.AddMenuEntry(
                                                        FText::FromString(TEXT("Add Shots to Scene")),
                                                        FText::FromString(TEXT("Enter shot marking mode for this scene")),
                                                        FSlateIcon(),
                                                        FUIAction(FExecuteAction::CreateLambda(
                                                            [this, SceneId = CapturedSceneId]()
                                                            { EnterShotMarkingMode(SceneId); }
                                                        ))
                                                    );

                                                    MenuBuilder.AddMenuEntry(
                                                        FText::FromString(TEXT("Build Scene Sequence")),
                                                        FText::FromString(TEXT("Build camera sequence for this scene")),
                                                        FSlateIcon(),
                                                        FUIAction(
                                                            FExecuteAction::CreateLambda(
                                                                [this, SceneId = CapturedSceneId]()
                                                                {
                                                                    FGASBlock* SceneBlock = nullptr;
                                                                    for (FGASBlock& Block : CurrentScript.Blocks)
                                                                    {
                                                                        if (Block.Id == SceneId && Block.Type == EGASBlockType::SceneHeading)
                                                                        {
                                                                            SceneBlock = &Block;
                                                                            break;
                                                                        }
                                                                    }
                                                                    if (!SceneBlock) return;

                                                                    if (SceneBlock->bSequenceBuilt)
                                                                    {
                                                                        const FText Title = FText::FromString(TEXT("Rebuild Scene Sequence?"));
                                                                        const FText Message = FText::FromString(TEXT("This scene already has a built sequence.\n\nRebuilding will overwrite any manual edits you made to the timeline.\n\nContinue?"));
                                                                        if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title) != EAppReturnType::Yes)
                                                                            return;
                                                                    }

                                                                    TArray<FRenderedParagraph> Rendered = ScriptPanel->GetRenderedParagraphs();
                                                                    TArray<FGASShotListSceneRow> SceneRows;
                                                                    BuildShotListFromScriptV2(CurrentScript, CurrentScript.SceneNumbering, Rendered, SceneRows);

                                                                    ULevelSequence* SceneSequence = GetOrCreateSceneSequence(*SceneBlock);
                                                                    if (!SceneSequence) return;

                                                                    AddCameraTracksToSceneSequence(SceneSequence, *SceneBlock, SceneRows);

                                                                    const FGASShotListSceneRow* FoundRow = SceneRows.FindByPredicate(
                                                                        [&SceneId](const FGASShotListSceneRow& Row) { return Row.SceneId == SceneId; });

                                                                    SceneBlock->bSequenceBuilt = true;
                                                                    SceneBlock->SequenceBuiltShotCount = FoundRow ? FoundRow->Shots.Num() : 0;

                                                                    MarkScriptDirty();
                                                                    EnsureScriptSaved();
                                                                    RebuildShotList();
                                                                }
                                                            ),
                                                            FCanExecuteAction::CreateLambda(
                                                                [this, SceneId = CapturedSceneId]()
                                                                {
                                                                    for (const FGASBlock& Block : CurrentScript.Blocks)
                                                                    {
                                                                        if (Block.Id == SceneId)
                                                                            return IsBlockingLevelOpen(Block.BlockingLevelPath);
                                                                    }
                                                                    return false;
                                                                }
                                                            )
                                                        )
                                                    );

                                                    MenuBuilder.AddMenuSeparator();

                                                    MenuBuilder.AddMenuEntry(
                                                        FText::FromString(TEXT("Build All Sequences")),
                                                        FText::FromString(TEXT("Build sequences for all scenes with blocking currently loaded")),
                                                        FSlateIcon(),
                                                        FUIAction(FExecuteAction::CreateLambda(
                                                            [this]()
                                                            {
                                                                TArray<FRenderedParagraph> Rendered = ScriptPanel->GetRenderedParagraphs();
                                                                TArray<FGASShotListSceneRow> SceneRows;
                                                                BuildShotListFromScriptV2(CurrentScript, CurrentScript.SceneNumbering, Rendered, SceneRows);

                                                                int32 BuiltCount = 0;
                                                                for (FGASBlock& Block : CurrentScript.Blocks)
                                                                {
                                                                    if (Block.Type != EGASBlockType::SceneHeading) continue;
                                                                    if (!IsBlockingLevelOpen(Block.BlockingLevelPath)) continue;

                                                                    ULevelSequence* SceneSequence = GetOrCreateSceneSequence(Block);
                                                                    if (!SceneSequence) continue;

                                                                    AddCameraTracksToSceneSequence(SceneSequence, Block, SceneRows);

                                                                    const FGASShotListSceneRow* FoundRow = SceneRows.FindByPredicate(
                                                                        [&Block](const FGASShotListSceneRow& Row) { return Row.SceneId == Block.Id; });

                                                                    Block.bSequenceBuilt = true;
                                                                    Block.SequenceBuiltShotCount = FoundRow ? FoundRow->Shots.Num() : 0;
                                                                    BuiltCount++;
                                                                }

                                                                if (BuiltCount > 0)
                                                                {
                                                                    MarkScriptDirty();
                                                                    EnsureScriptSaved();
                                                                    RebuildShotList();
                                                                }
                                                            }
                                                        ))
                                                    );

                                                    MenuBuilder.AddMenuSeparator();

                                                    MenuBuilder.AddMenuEntry(
                                                        FText::FromString(TEXT("Edit Blocking")),
                                                        FText::FromString(TEXT("Open existing blocking level")),
                                                        FSlateIcon(),
                                                        FUIAction(FExecuteAction::CreateLambda(
                                                            [this, SceneId = CapturedSceneId]()
                                                            {
                                                                for (const FGASBlock& Block : CurrentScript.Blocks)
                                                                {
                                                                    if (Block.Id == SceneId && Block.bSequenceBuilt)
                                                                    {
                                                                        const FText Title = FText::FromString(TEXT("Edit Blocking?"));
                                                                        const FText Message = FText::FromString(TEXT("This scene has a built sequence.\n\nChanges to blocking may affect your shots and will mark the sequence as out of date.\n\nContinue?"));
                                                                        if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title) != EAppReturnType::Yes)
                                                                            return;
                                                                        break;
                                                                    }
                                                                }
                                                                this->PromoteNonBlockingShotsToBlocking(SceneId);
                                                                this->OnStartBlockingScene(SceneId);
                                                            }
                                                        ))
                                                    );

                                                    MenuBuilder.AddMenuEntry(
                                                        FText::FromString(TEXT("Edit Cast")),
                                                        FText::FromString(TEXT("Modify cast for this blocking scene.")),
                                                        FSlateIcon(),
                                                        FUIAction(FExecuteAction::CreateLambda(
                                                            [this, SceneId = CapturedSceneId]()
                                                            { this->OpenCastWindowForScene(SceneId); }
                                                        ))
                                                    );

                                                    MenuBuilder.AddMenuEntry(
                                                        FText::FromString(TEXT("Delete Blocking")),
                                                        FText::FromString(TEXT("Delete blocking level for this scene")),
                                                        FSlateIcon(),
                                                        FUIAction(FExecuteAction::CreateLambda(
                                                            [this, SceneId = CapturedSceneId]()
                                                            { this->OnDeleteBlockingScene(SceneId); }
                                                        ))
                                                    );
                                                }

                                                FSlateApplication::Get().PushMenu(
                                                    AsShared(), FWidgetPath(),
                                                    MenuBuilder.MakeWidget(),
                                                    MouseEvent.GetScreenSpacePosition(),
                                                    FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
                                                );
                                                return FReply::Handled();
                                            }

                                            if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
                                            {
                                                if (ScriptPanel.IsValid())
                                                    ScriptPanel->ScrollToBlockId(Scene.SceneId);
                                                for (const FGASBlock& Block : CurrentScript.Blocks)
                                                {
                                                    if (Block.Id == Scene.SceneId)
                                                    {
                                                        SyncSequencerToScene(Block.Text);
                                                        break;
                                                    }
                                                }
                                                return FReply::Handled();
                                            }

                                            return FReply::Unhandled();
                                        }
                                    )
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(Scene.SceneHeading.ToUpper()))
                                    ]
                            ]
                    ]

                // 1/8
                + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(CW_Eighths)
                            [SNew(STextBlock).Text(FText::FromString(Scene.FormattedLength))]
                    ]

                    // Time
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(CW_Time)
                            [SNew(STextBlock).Text(FText::FromString(Scene.TimeOfDayOverride.ToUpper()))]
                    ]

                    // Set
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(CW_Set)
                            [
                                SNew(STextBlock)
                                    .Text_Lambda([this, SceneId = Scene.SceneId]()
                                        {
                                            for (const FGASBlock& Block : CurrentScript.Blocks)
                                            {
                                                if (Block.Id == SceneId)
                                                {
                                                    if (!Block.AssignedSetId.IsNone())
                                                    {
                                                        for (const FGASSetDescriptor& Set : FGASSetDiscovery::GetAvailableSets())
                                                        {
                                                            if (Set.SetId == Block.AssignedSetId)
                                                                return Set.DisplayName;
                                                        }
                                                        return FText::FromName(Block.AssignedSetId);
                                                    }
                                                    break;
                                                }
                                            }
                                            return FText::GetEmpty();
                                        })
                                    .ColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.2f, 1.f))
                                    .ToolTipText_Lambda([this, SceneId = Scene.SceneId]()
                                        {
                                            for (const FGASBlock& Block : CurrentScript.Blocks)
                                            {
                                                if (Block.Id == SceneId)
                                                    return Block.BlockingLevelPath.IsEmpty()
                                                    ? FText::GetEmpty()
                                                    : FText::FromString(Block.BlockingLevelPath);
                                            }
                                            return FText::GetEmpty();
                                        })
                            ]
                    ]

                // Notes
                + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(CW_Notes)
                            [
                                SNew(SEditableTextBox)
                                    .Text_Lambda([this, SceneId = Scene.SceneId]()
                                        {
                                            for (const FGASBlock& Block : CurrentScript.Blocks)
                                            {
                                                if (Block.Id == SceneId)
                                                {
                                                    const FString* Found = Block.Metadata.Find(TEXT("EnvNotes"));
                                                    return Found
                                                        ? FText::FromString(*Found)
                                                        : FText::GetEmpty();
                                                }
                                            }
                                            return FText::GetEmpty();
                                        })
                                    .OnTextCommitted_Lambda([this, SceneId = Scene.SceneId](const FText& NewText, ETextCommit::Type)
                                        {
                                            for (FGASBlock& Block : CurrentScript.Blocks)
                                            {
                                                if (Block.Id == SceneId)
                                                {
                                                    Block.Metadata.Add(TEXT("EnvNotes"), NewText.ToString());
                                                    MarkScriptDirty();
                                                    SaveScriptToJson();
                                                    break;
                                                }
                                            }
                                        })
                            ]
                    ]
            ];

        // =====================================================
        // SHOT ROWS (if expanded)
        // =====================================================
        if (ExpandedScenes.Contains(Scene.SceneId))
        {
            // Shot header
            ShotListContainer->AddSlot()
                .AutoHeight()
                .Padding(CW_Expand + 2.f, 4.f, 4.f, 2.f)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_Lock)[SNew(STextBlock).Text(FText::GetEmpty())]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_Shot)[SNew(STextBlock).Text(FText::FromString(TEXT("SHOT"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_Type)[SNew(STextBlock).Text(FText::FromString(TEXT("TYPE"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_Move)[SNew(STextBlock).Text(FText::FromString(TEXT("MOV"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_SEighths)[SNew(STextBlock).Text(FText::FromString(TEXT("1/8"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_Desc)[SNew(STextBlock).Text(FText::FromString(TEXT("DESCRIPTION"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_Lens)[SNew(STextBlock).Text(FText::FromString(TEXT("LENS"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_Camera)[SNew(STextBlock).Text(FText::FromString(TEXT("CAMERA"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(CW_SNotes)[SNew(STextBlock).Text(FText::FromString(TEXT("NOTES"))).ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))]]
                ];

            // Shot rows
            for (const FGASShotListShotRow& Shot : Scene.Shots)
            {
                ShotListContainer->AddSlot()
                    .AutoHeight()
                    .Padding(CW_Expand + 2.f, 2.f, 4.f, 2.f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(CW_Shot)
                                    [SNew(STextBlock).Text(FText::FromString(Shot.ShotNumber))]
                            ]

                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(CW_Type)
                                    [
                                        SNew(STextBlock).Text(
                                            Shot.ShotType.IsEmpty()
                                            ? FText::FromString(TEXT("—"))
                                            : FText::FromString(Shot.ShotType))
                                    ]
                            ]

                        + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(CW_Move)
                                    [
                                        SNew(STextBlock)
                                            .Text(Shot.bCameraMoves ? FText::FromString(TEXT("✓")) : FText::FromString(TEXT("—")))
                                            .ColorAndOpacity(Shot.bCameraMoves
                                                ? FLinearColor(0.2f, 1.f, 0.2f, 1.f)
                                                : FLinearColor(0.4f, 0.4f, 0.4f, 1.f))
                                    ]
                            ]

                        + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(CW_SEighths)
                                    [
                                        SNew(STextBlock).Text(
                                            Shot.LengthEighths > 0
                                            ? FText::FromString(GAS_FormatPagesEighths(Shot.LengthEighths))
                                            : FText::FromString(TEXT("—")))
                                    ]
                            ]

                        + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(CW_Desc)
                                    [
                                        SNew(SEditableTextBox)
                                            .Text(FText::FromString(Shot.Description))
                                            .OnTextCommitted_Lambda([this, Shot](const FText& NewText, ETextCommit::Type)
                                                { this->UpdateShotDescription(Shot.ShotId, NewText.ToString()); })
                                    ]
                            ]

                        + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(CW_Lens)
                                    [
                                        SNew(STextBlock).Text(
                                            Shot.Lens.IsEmpty()
                                            ? FText::FromString(TEXT("—"))
                                            : FText::FromString(Shot.Lens))
                                    ]
                            ]

                        + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(CW_Camera)
                                    [
                                        SNew(STextBlock)
                                            .Text(Shot.Camera.IsEmpty()
                                                ? FText::FromString(TEXT("—"))
                                                : FText::FromString(Shot.Camera))
                                            .WrapTextAt(CW_Camera - 2.f)
                                    ]
                            ]

                        + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBox).WidthOverride(CW_SNotes)
                                    [
                                        SNew(SEditableTextBox)
                                            .Text(FText::FromString(Shot.Notes))
                                            .OnTextCommitted_Lambda([this, Shot](const FText& NewText, ETextCommit::Type)
                                                { this->UpdateShotNotes(Shot.ShotId, NewText.ToString()); })
                                    ]
                            ]
                    ];
            }
        }
    }
}

void SGAS_ScriptTab::ScrollToScene(const FGASShotDefinitionListRow& Scene)
{
    if (!ScriptPanel.IsValid())
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("ScrollToScene: ScriptPanel invalid"));
        return;
    }

    if (!CurrentScript.Blocks.IsValidIndex(Scene.SceneBlockIndex))
    {
        UE_LOG(LogGASPrePro, Warning,
            TEXT("ScrollToScene: invalid SceneBlockIndex %d"),
            Scene.SceneBlockIndex
        );
        return;
    }

    const FString& BlockId =
        CurrentScript.Blocks[Scene.SceneBlockIndex].Id;

    UE_LOG(LogGASPrePro, Warning,
        TEXT("ScrollToScene: SceneBlockIndex=%d  BlockId=%s"),
        Scene.SceneBlockIndex,
        *BlockId
    );

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[SCENE VERIFY] Index=%d Id=%s"),
        Scene.SceneBlockIndex,
        *CurrentScript.Blocks[Scene.SceneBlockIndex].Id
    );


    // ✅ THIS IS THE JUMP
    ScriptPanel->ScrollToBlockId(BlockId);

}

void SGAS_ScriptTab::UpdateShotDescription(
    const FGuid& ShotId,
    const FString& NewDescription)
{
    if (!ScriptPanel.IsValid())
    {
        return;
    }

    ScriptPanel->ModifyShotMarkerMetadata(
        ShotId,
        TEXT("Description"),
        NewDescription
    );

    MarkScriptDirty();
}

void SGAS_ScriptTab::UpdateShotNotes(
    const FGuid& ShotId,
    const FString& NewNotes)
{
    if (!ScriptPanel.IsValid())
    {
        return;
    }

    ScriptPanel->ModifyShotMarkerMetadata(
        ShotId,
        TEXT("Notes"),
        NewNotes
    );

    MarkScriptDirty();
}


void SGAS_ScriptTab::SaveScriptToJson()
{

    const FString JsonPath = GetAuthoritativeScriptJsonPath();

    if (JsonPath.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Error, TEXT("SCRIPT TAB: JsonPath is EMPTY — aborting save"));
        return;
    }

    UE_LOG(LogGASPrePro, Warning, TEXT("SCRIPT TAB: JSON save path = %s"), *JsonPath);

    // ------------------------------------------------------------
    // Build JSON from FGASScript
    // ------------------------------------------------------------
    FString OutputString;
    if (!FJsonObjectConverter::UStructToJsonObjectString(CurrentScript, OutputString))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("SCRIPT TAB: Failed to convert script to JSON"));
        return;
    }

    // ------------------------------------------------------------
    // Ensure directory exists
    // ------------------------------------------------------------
    const FString Directory = FPaths::GetPath(JsonPath);

    if (!IFileManager::Get().DirectoryExists(*Directory))
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("SCRIPT TAB: Creating directory: %s"), *Directory);

        if (!IFileManager::Get().MakeDirectory(*Directory, true))
        {
            UE_LOG(LogGASPrePro, Error, TEXT("SCRIPT TAB: Failed to create directory: %s"), *Directory);
            return;
        }
    }

    // ------------------------------------------------------------
    // Save file
    // ------------------------------------------------------------
    if (!FFileHelper::SaveStringToFile(OutputString, *JsonPath))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("SCRIPT TAB: Failed to write JSON file: %s"), *JsonPath);
        return;
    }

    UE_LOG(LogGASPrePro, Log, TEXT("SCRIPT TAB: Saved script JSON → %s"), *JsonPath);

    // ------------------------------------------------------------
    // Clear dirty state
    // ------------------------------------------------------------
    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.ClearToolDirty();
    bIsScriptDirty = false;
}


FReply SGAS_ScriptTab::OnClearScriptConfirm()
{
    const FText DialogText = FText::FromString(
        TEXT("This will clear your entire script and marks.\nAre you sure?")
    );

    const FText DialogTitle = FText::FromString(TEXT("Confirm Clear"));

    EAppReturnType::Type Result = FMessageDialog::Open(
        EAppMsgType::OkCancel,
        DialogText,
        &DialogTitle
    );

    if (Result != EAppReturnType::Ok)
    {
        return FReply::Handled();
    }

    // ------------------------------------------------------------
    // Check for blocking levels
    // ------------------------------------------------------------
    bool bHasBlockingLevels = false;

    for (const FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Type == EGASBlockType::SceneHeading &&
            !Block.BlockingLevelPath.IsEmpty())
        {
            bHasBlockingLevels = true;
            break;
        }
    }

    if (bHasBlockingLevels)
    {
        const FText BlockingText = FText::FromString(
            TEXT("This will also DELETE all blocking levels associated with this script.\nContinue?")
        );

        const FText BlockingTitle = FText::FromString(TEXT("Delete Blocking Levels?"));

        EAppReturnType::Type BlockingResult = FMessageDialog::Open(
            EAppMsgType::OkCancel,
            BlockingText,
            &BlockingTitle
        );

        if (BlockingResult != EAppReturnType::Ok)
        {
            return FReply::Handled();
        }

        // ------------------------------------------------------------
        // Delete all blocking levels
        // ------------------------------------------------------------
        for (FGASBlock& Block : CurrentScript.Blocks)
        {
            if (Block.Type != EGASBlockType::SceneHeading)
            {
                continue;
            }

            if (Block.BlockingLevelPath.IsEmpty())
            {
                continue;
            }

            // --------------------------------------------------------
            // If this blocking level is currently open, switch first
            // --------------------------------------------------------
            if (GEditor)
            {
                UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();

                if (CurrentWorld)
                {
                    const FString CurrentMapPath =
                        CurrentWorld->GetOutermost()->GetName();

                    if (CurrentMapPath == Block.BlockingLevelPath)
                    {
                        FEditorFileUtils::LoadMap(TEXT("/Engine/Maps/Templates/Template_Default"), false, true);
                    }
                }
            }

            // --------------------------------------------------------
            // Now safely load and delete the asset
            // --------------------------------------------------------
            UObject* Asset = LoadObject<UObject>(
                nullptr,
                *Block.BlockingLevelPath
            );

            if (!Asset)
            {
                continue;
            }

            TArray<UObject*> AssetsToDelete;
            AssetsToDelete.Add(Asset);

            ObjectTools::DeleteObjects(AssetsToDelete);

            Block.BlockingLevelPath.Empty();
            Block.AssignedSetId = NAME_None;
        }

    }

    // Finally clear script
    OnClearScriptClicked();

    return FReply::Handled();
}

FReply SGAS_ScriptTab::OnToggleAddMode()
{
    bIsAddMode = !bIsAddMode;

    UE_LOG(LogGASPrePro, Warning,
        TEXT("ADD MODE TOGGLED: %s"),
        bIsAddMode ? TEXT("ON") : TEXT("OFF")
    );

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetAddMode(bIsAddMode);
    }

    return FReply::Handled();
}

void SGAS_ScriptTab::LoadScriptFromJsonIfExists()
{
    const FString ScriptPath = GetAuthoritativeScriptJsonPath();

    UE_LOG(LogGASPrePro, Verbose, TEXT("[GAS] Looking for script JSON at: %s"), *ScriptPath);

    if (!FPaths::FileExists(ScriptPath))
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("[GAS] No saved script JSON found."));
        return;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *ScriptPath))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("[GAS] Failed to read script JSON: %s"), *ScriptPath);
        return;
    }

    FGASScript Loaded;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonString, &Loaded, 0, 0))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("[GAS] Failed to parse script JSON: %s"), *ScriptPath);
        return;
    }

    CurrentScript = MoveTemp(Loaded);
    BuildCastListFromScript();
    RebuildCastUI();


    UE_LOG(
        LogGASPrePro,
        Verbose,
        TEXT("[GAS] Loaded script JSON OK: Blocks=%d PageBreaks=%d Markers=%d"),
        CurrentScript.Blocks.Num(),
        CurrentScript.UserPageBreaks.Num(),
        CurrentScript.Markers.Num()
    );

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetScript(&CurrentScript);

        TArray<FRenderedParagraph> Rendered =
            ScriptLayoutEngine::LayoutScript(
                CurrentScript.Blocks,
                CachedScriptPanelWidth,
                CurrentScript.UserPageBreaks,
                CurrentScript.SceneNumbering
            );

        ScriptPanel->SetRenderedScript(Rendered);
    }

    RebuildShotList();

}

FString SGAS_ScriptTab::GetAuthoritativeScriptJsonPath() const
{
    if (!ActiveProject ||
        ActiveProject->ProjectName.IsEmpty())
    {
        return TEXT("");
    }

    FString BaseDir =
        FPaths::ProjectSavedDir() + TEXT("GAS/");

    FString ProjectDir =
        BaseDir +
        ActiveProject->ProjectName +
        TEXT("/");

    IFileManager::Get().MakeDirectory(*ProjectDir, true);

    return ProjectDir + TEXT("Script.gasjson");
}

void SGAS_ScriptTab::ScrollToSceneBlock(const FString& BlockId)
{
    if (!ScriptPanel.IsValid())
        return;

    ScriptPanel->ScrollToBlockId(BlockId);
}


void SGAS_ScriptTab::ConfigurePreviewCameraFilmback(UCineCameraComponent* InCamera) const
{
    if (!InCamera) return;

    InCamera->Filmback.SensorWidth = 36.0f;
    InCamera->Filmback.SensorHeight = 20.25f;

    InCamera->bConstrainAspectRatio = true;
}

FVector2D SGAS_ScriptTab::GetPreviewWidgetSize(float InWidth) const
{
    return FVector2D(InWidth, InWidth / (16.0f / 9.0f));
}


void SGAS_ScriptTab::HandleMapOpened(const FString& Filename, bool bAsTemplate)
{
    bLoadingBlockingLevel = false;

    // ------------------------------------------------------------
    // Pending shot delete after opening the correct blocking level
    // ------------------------------------------------------------
    if (bPendingDeleteShotAfterMapOpen)
    {
        if (PendingDeleteShotLevelPath.IsEmpty() ||
            Filename.Contains(PendingDeleteShotLevelPath))
        {
            const FString MarkerId = PendingDeleteShotMarkerId;

            bPendingDeleteShotAfterMapOpen = false;
            PendingDeleteShotMarkerId.Empty();
            PendingDeleteShotLevelPath.Empty();

            UpdatePendingActionWindow(TEXT("Deleting shot and camera..."));
            DeleteShotMarkerNow(MarkerId);
            return;
        }
    }

    // ------------------------------------------------------------
    // Pending shot intent open after blocking level loads
    // ------------------------------------------------------------
    if (!PendingShotIntentMarkerId.IsEmpty())
    {
        if (Filename.Contains(
            FPaths::GetBaseFilename(PendingShotIntentSceneId.IsEmpty()
                ? TEXT("")
                : GetBlockingLevelPath(PendingShotIntentSceneId))))
        {
            const FString IntentMarkerId = PendingShotIntentMarkerId;
            const FString IntentSceneId = PendingShotIntentSceneId;

            PendingShotIntentMarkerId.Empty();
            PendingShotIntentSceneId.Empty();

            ClosePendingActionWindow();

            const FString CapturedMarkerId = IntentMarkerId;
            const FString CapturedSceneId = IntentSceneId;

            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda(
                    [this, CapturedMarkerId, CapturedSceneId](float) -> bool
                    {
                        if (ScriptPanel.IsValid())
                        {
                            ScriptPanel->OpenShotIntentPopup(
                                CapturedMarkerId,
                                CapturedSceneId,
                                true,
                                false
                            );
                        }
                        return false;
                    }
                ),
                0.5f
            );
        }
    }

    // ------------------------------------------------------------
    // Pending shot mode resume
    // ------------------------------------------------------------
    if (!PendingShotModeLevelPath.IsEmpty() &&
        Filename.Contains(PendingShotModeLevelPath))
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMode] Map loaded, resuming"));

        const FString SceneId = PendingShotModeSceneId;

        PendingShotModeSceneId.Empty();
        PendingShotModeLevelPath.Empty();

        if (!SceneId.IsEmpty())
        {
            bIsResumingShotMode = true;
            EnterShotMarkingMode(SceneId);
            bIsResumingShotMode = false;

            RehydrateShotCamerasForScene(SceneId);
        }

        PendingShotIntentMarkerId.Empty();
        PendingShotIntentSceneId.Empty();

        ClosePendingActionWindow();
        return;
    }

    // ------------------------------------------------------------
    // Blocking flow auto-open cast window
    // ONLY for true blocking startup flow
    // ------------------------------------------------------------
    if (bBlockingActive && bPendingAutoOpenCastWindow)
    {
        bPendingAutoOpenCastWindow = false;

        const FString SceneId = ActiveBlockingSceneId.ToString();
        ClosePendingActionWindow();

        if (!SceneId.IsEmpty())
        {
            if (GActiveBlockingSequence)
            {
                MuteNonBlockingTracksForEditing(GActiveBlockingSequence, GEditor->GetEditorWorldContext().World());
            }

            OpenCastWindowForScene(SceneId);
        }

        // Open scene sequence AFTER level is confirmed loaded
        if (GActiveBlockingSequence)
        {
            const FString SeqPath = GActiveBlockingSequence->GetPathName();
            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([this, SeqPath](float) -> bool
                    {
                        ULevelSequence* Seq =
                            LoadObject<ULevelSequence>(nullptr, *SeqPath);
                        if (Seq && GEditor)
                        {
                            UAssetEditorSubsystem* EdSub =
                                GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
                            if (EdSub)
                                EdSub->OpenEditorForAsset(Seq);
                        }
                        return false;
                    }),
                0.5f
            );
        }

        return;
    }

    ClosePendingActionWindow();
}

FString SGAS_ScriptTab::GetBlockingLevelPath(const FString& SceneId) const
{
    const FGASScript& LocalScript = GetScript();

    for (const FGASBlock& Block : LocalScript.Blocks)
    {
        if (Block.Id == SceneId)
        {
            return Block.BlockingLevelPath;
        }
    }

    return FString();
}

bool SGAS_ScriptTab::IsBlockingLevelOpen(const FString& LevelPath)
{
#if WITH_EDITOR
    if (!GEditor) return false;

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return false;

    const FString CurrentMap = World->GetOutermost()->GetName();

    return !LevelPath.IsEmpty() && CurrentMap == LevelPath;
#else
    return false;
#endif
}

void SGAS_ScriptTab::OpenBlockingLevel(const FString& LevelPath)
{
    if (LevelPath.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMode] Invalid level path"));
        return;
    }

    if (!GEditor)
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMode] GEditor invalid"));
        return;
    }

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[ShotMode] Loading level: %s"), *LevelPath);

    FEditorFileUtils::LoadMap(LevelPath, false, true);
}

void SGAS_ScriptTab::OpenBlockingLevelIfNeeded(const FString& SceneId)
{
#if WITH_EDITOR
    const FString LevelPath = GetBlockingLevelPath(SceneId);

    if (LevelPath.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Warning,
            TEXT("[ShotMode] No blocking level for scene %s"), *SceneId);
        return;
    }

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[ShotMode] Loading level: %s"), *LevelPath);

    FEditorFileUtils::LoadMap(LevelPath, false, true);
#endif
}


FString SGAS_ScriptTab::GetSequencePath(const FString& SceneId) const
{
    const FGASScript& LocalScript = GetScript();

    for (const FGASBlock& Block : LocalScript.Blocks)
    {
        if (Block.Id == SceneId)
        {
            return Block.MasterSequencePath;
        }
    }

    return FString();
}

void SGAS_ScriptTab::ResetShotModeState()
{
    // NOTE: bShotAddArmed is intentionally NOT reset here.
    // Shot marking mode stays armed until the user clicks the icon again.
    bIsShotRangeDragging = false;
    bAllowShotHoverPreview = true;  // re-enable hover for next shot
    bIsEditingShot = false;
    ActiveCameraModeSceneId.Empty();
    ShotRangeStartParagraph = INDEX_NONE;
    Invalidate(EInvalidateWidget::LayoutAndVolatility);
    UE_LOG(LogGASPrePro, Warning,
        TEXT("[ShotMode] RESET (armed state preserved)"));
}

void SGAS_ScriptTab::EnterShotMarkingMode(const FString& SceneId)
{
    ResetShotModeState();

    if (SceneId.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMode] Empty SceneId"));
        return;
    }

    const FString BlockingPath = GetBlockingLevelPath(SceneId);

    if (BlockingPath.IsEmpty())
    {
        UE_LOG(LogGASPrePro, Warning,
            TEXT("[ShotMode] No scene level exists for Scene=%s"),
            *SceneId);

        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(
                TEXT("This scene does not have a scene level yet.\n\nPlease create blocking first.")
            )
        );

        return;
    }

    // --------------------------------------------------
    // If correct blocking level is NOT open yet, open it and resume later
    // --------------------------------------------------
    if (!bIsResumingShotMode && !IsBlockingLevelOpen(BlockingPath))
    {
        UE_LOG(LogGASPrePro, Warning,
            TEXT("[ShotMode] Opening level FIRST Scene=%s Path=%s"),
            *SceneId, *BlockingPath);

        PendingShotModeSceneId = SceneId;
        PendingShotModeLevelPath = BlockingPath;
        bPendingAutoOpenCastWindow = false;

        ShowPendingActionWindow(TEXT("Opening blocking scene..."));
        OpenBlockingLevel(BlockingPath);
        return;
    }

    UE_LOG(LogGASPrePro, Warning,
        TEXT("[ShotMode] ENTER Scene=%s"), *SceneId);

    // --------------------------------------------------
    // MINIMAL SAFE ENTRY
    // Do NOT touch preview or sequencer here yet
    // --------------------------------------------------
    ActiveCameraModeSceneId = SceneId;

    //RehydrateShotCamerasForScene(SceneId);
    BindToExistingShotCameras();

    bShotAddArmed = true;
    bIsShotRangeDragging = false;
    ShotRangeStartParagraph = INDEX_NONE;
    bAllowShotHoverPreview = true;

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetShotAddArmed(true);
    }

    Invalidate(EInvalidateWidget::LayoutAndVolatility);
    ClosePendingActionWindow();
}

SGAS_ScriptTab::~SGAS_ScriptTab()
{
    UE_LOG(LogTemp, Error, TEXT("SGAS_ScriptTab DESTROYED"));

    if (OnMapOpenedHandle.IsValid())
    {
        FEditorDelegates::OnMapOpened.Remove(OnMapOpenedHandle);
        OnMapOpenedHandle.Reset();
    }
}



FSlateColor SGAS_ScriptTab::GetShotButtonTint() const
{
    return bShotAddArmed
        ? FLinearColor(0.25f, 0.6f, 1.f, 1.0f)   // 🔵 shot mode armed
        : FLinearColor(0.7f, 0.7f, 0.7f, 0.8f);  // ⚪ idle
}

FGASScript* SGAS_ScriptTab::GetCurrentScript()
{
    return &CurrentScript;
}

void SGAS_ScriptTab::SetActiveBlockingShot(const FGuid& ShotId)
{
    if (!ShotId.IsValid())
    {
        return;
    }

    FGASMarker* FoundMarker = CurrentScript.Markers.FindByPredicate(
        [&](const FGASMarker& Marker)
        {
            return Marker.MarkerGuid == ShotId;
        }
    );

    if (!FoundMarker)
    {
        return;
    }

    if (!FoundMarker->IsBlocking())
    {
        return;
    }

    ActiveBlockingShotId = ShotId;

    UE_LOG(LogGASPrePro, Warning,
        TEXT("SetActiveBlockingShot: %s"),
        *ActiveBlockingShotId.ToString());

    FGASShotIntent* Shot = CurrentScript.ShotIntents.Find(FoundMarker->Id);
    if (!Shot)
    {
        return;
    }

    if (IsValid(Shot->CameraActor))
    {
        AGAS_ShotCameraActor* Cam = Shot->CameraActor;

        if (Cam)
        {
            if (!Shot->CameraLocation.IsNearlyZero() || !Shot->CameraRotation.IsNearlyZero())
            {
                Cam->SetActorLocationAndRotation(
                    Shot->CameraLocation,
                    Shot->CameraRotation
                );

                if (Cam->CineCamera)
                {
                    Cam->CineCamera->SetCurrentFocalLength(
                        Shot->CameraFocalLength
                    );
                }

                UE_LOG(LogTemp, Warning, TEXT("FINAL CAMERA APPLIED (SetActive): %s"),
                    *Shot->CameraLocation.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("SKIPPING CAMERA APPLY (no saved data yet)"));
            }
        }
    }
}

FGuid SGAS_ScriptTab::GetActiveBlockingShot() const
{
    return ActiveBlockingShotId;
}

void SGAS_ScriptTab::NotifyShotCameraBound(const FGuid& ShotId)
{
    for (FGASMarker& Marker : CurrentScript.Markers)
    {
        if (Marker.MarkerType == EGASMarkerType::ShotMarker &&
            Marker.MarkerGuid == ShotId)
        {
            if (!Marker.IsBlocking())
            {
                return;
            }

            Marker.PromoteToCameraPlaced();

            MarkScriptDirty();
            OnSaveScript();

            UE_LOG(LogGASPrePro, Warning,
                TEXT("Shot promoted to CameraPlaced: %s"),
                *ShotId.ToString());

            break;
        }
    }
}

#if WITH_EDITOR

void SGAS_ScriptTab::BindToExistingShotCameras()
{
    if (!GEditor)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return;
    }

    // --------------------------------------------------
    // Reset authoritative bound set each entry
    // --------------------------------------------------
    BoundShotCameraSet.Empty();

    int32 BindCount = 0;

    for (TActorIterator<AGAS_ShotCameraActor> It(World); It; ++It)
    {
        AGAS_ShotCameraActor* Cam = *It;

        if (!IsValid(Cam))
        {
            continue;
        }

        // --------------------------------------------------
        // Prevent duplicate delegate binding
        // --------------------------------------------------
        Cam->OnShotCameraMoved.RemoveAll(this);

        Cam->OnShotCameraMoved.AddSP(
            SharedThis(this),
            &SGAS_ScriptTab::HandleShotCameraMoved
        );

        BoundShotCameraSet.Add(Cam);

        ++BindCount;

        UE_LOG(LogTemp, Warning,
            TEXT("BOUND SHOT CAMERA: %s"),
            *GetNameSafe(Cam));
    }

    UE_LOG(LogTemp, Warning,
        TEXT("BindToExistingShotCameras COMPLETE | Count=%d"),
        BindCount);
}

void SGAS_ScriptTab::HandleShotCameraMoved(const FString& MarkerId, const FTransform& NewTransform)
{
    if (!bBlockingActive)
    {
        return;
    }
    FGASMarker* Found = nullptr;

    for (FGASMarker& M : CurrentScript.Markers)
    {
        if (M.Id == MarkerId)
        {
            Found = &M;
            break;
        }
    }

    if (!Found)
    {
        UE_LOG(LogGASPrePro, Warning, TEXT("HandleShotCameraMoved: Marker not found: %s"), *MarkerId);
        return;
    }

    if (!Found->IsBlocking())
    {
        return;
    }

    // Respect shot lock — don't update camera if shot is locked
    if (Found->SpatialState == EGASShotSpatialState::Locked)
    {
        return;
    }

    // Authoritative mutation (sets bHasCamera, stores transform, promotes spatial state)
    Found->BindCameraTransform(NewTransform);

    // Your existing dirty + save pipeline
    MarkScriptDirty();
    EnsureScriptSaved();
}

void SGAS_ScriptTab::UpdateShotIntentCameraFromPreview(
    const FString& MarkerId,
    const FVector& Location,
    const FRotator& Rotation,
    float FocalLength
)
{
    if (!Script.IsValid())
    {
        return;
    }

    FGASShotIntent* Shot = Script->ShotIntents.Find(MarkerId);
    if (!Shot)
    {
        UE_LOG(LogTemp, Warning, TEXT("UpdateShotIntentCameraFromPreview: Shot NOT FOUND for %s"), *MarkerId);
        return;
    }

    Shot->CameraLocation = Location;
    Shot->CameraRotation = Rotation;
    Shot->CameraFocalLength = FocalLength;

}

void SGAS_ScriptTab::RehydrateShotCamerasForScene(const FString& SceneBlockId)
{
    static bool bRehydrating = false;

    if (bRehydrating)
    {
        return;
    }

    TGuardValue<bool> RehydrateGuard(bRehydrating, true);

    if (!GEditor)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return;
    }

    // Build quick lookup of existing camera actors by MarkerId
    TMap<FString, AGAS_ShotCameraActor*> ExistingByMarkerId;

    for (TActorIterator<AGAS_ShotCameraActor> It(World); It; ++It)
    {
        AGAS_ShotCameraActor* Cam = *It;
        if (!IsValid(Cam))
        {
            continue;
        }

        if (!Cam->MarkerId.IsEmpty())
        {
            ExistingByMarkerId.Add(Cam->MarkerId, Cam);
        }
    }

    // Spawn / snap cameras based on authoritative marker data
    for (const FGASMarker& Marker : CurrentScript.Markers)
    {


        // Only shot markers for this scene
        if (Marker.MarkerType != EGASMarkerType::ShotMarker)
        {
            continue;
        }

        if (!Marker.IsBlocking())
        {
            continue;
        }

        if (Marker.BlockId != SceneBlockId)
        {
            continue;
        }

        if (!Marker.bHasCamera)
        {
            continue;
        }

        const FString& MarkerId = Marker.Id;

        if (AGAS_ShotCameraActor** FoundCamPtr = ExistingByMarkerId.Find(MarkerId))
        {
            AGAS_ShotCameraActor* FoundCam = *FoundCamPtr;

            if (IsValid(FoundCam))
            {
                FoundCam->SetActorTransform(Marker.CameraTransform);

                if (FGASShotIntent* Intent =
                    CurrentScript.ShotIntents.Find(MarkerId))
                {
                    Intent->CameraActor = FoundCam;

                    UE_LOG(LogTemp, Error,
                        TEXT("REBIND EXISTING CAMERA: %s"),
                        *FoundCam->GetName());
                }
            }

            continue;
        }

        if (ExistingByMarkerId.Contains(MarkerId))
        {
            continue;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AGAS_ShotCameraActor* NewCam =
            World->SpawnActor<AGAS_ShotCameraActor>(
                AGAS_ShotCameraActor::StaticClass(),
                Marker.CameraTransform,
                Params
            );

        if (!IsValid(NewCam))
        {

            continue;
        }

        NewCam->MarkerId = MarkerId;

        if (FGASShotIntent* Intent =
            CurrentScript.ShotIntents.Find(MarkerId))
        {
            Intent->CameraActor = NewCam;

            UE_LOG(LogTemp, Error,
                TEXT("REBIND NEW CAMERA: %s"),
                *NewCam->GetName());
        }

        NewCam->SetActorLabel(
            FString::Printf(
                TEXT("SHOT_%s"),
                *Marker.GetShotLabel(CurrentScript.ShotNumberingPolicy)
            )
        );

        NewCam->SetActorHiddenInGame(false);
        NewCam->SetIsTemporarilyHiddenInEditor(false);


        ExistingByMarkerId.Add(MarkerId, NewCam);
    }

}


#endif

// --------------------------------------------------
// SCRIPT MARKERS
// --------------------------------------------------

void SGAS_ScriptTab::BuildSceneMarkersFromScriptTiming()
{
    if (!ActiveProject)
    {
        return;
    }

    const FString ProjectName =
        ActiveProject->ProjectName;

    const FString SequencePath =
        FString::Printf(
            TEXT("/Game/PrePro/Projects/%s/Sequences/MASTER_%s"),
            *ProjectName,
            *ProjectName
        );

    ULevelSequence* Sequence =
        LoadObject<ULevelSequence>(
            nullptr,
            *SequencePath
        );

    if (!Sequence)
    {
        UE_LOG(LogTemp, Error,
            TEXT("BuildSceneMarkersFromScriptTiming: MASTER sequence missing"));
        return;
    }

    UMovieScene* MovieScene =
        Sequence->GetMovieScene();

    if (!MovieScene)
    {
        return;
    }

    MovieScene->Modify();
    MovieScene->DeleteMarkedFrames();

    // ------------------------------------------------------------
    // BUILD SCENE ROWS (authoritative eighths from layout)
    // ------------------------------------------------------------

    constexpr float SecondsPerEighth = 7.5f;

    TArray<FGASShotListSceneRow> SceneRows;

    BuildShotListFromScriptV2(
        CurrentScript,
        CurrentScript.SceneNumbering,
        ScriptPanel->GetRenderedParagraphs(),
        SceneRows
    );

    // ------------------------------------------------------------
    // PLACE MARKERS
    // ------------------------------------------------------------

    int32 CurrentFrame = 0;

    const FFrameRate TickResolution =
        MovieScene->GetTickResolution();

    const float FPS =
        TickResolution.AsDecimal();

    for (const FGASBlock& Block : CurrentScript.Blocks)
    {
        if (Block.Type != EGASBlockType::SceneHeading)
        {
            continue;
        }

        // Look up authoritative eighths from layout
        float SceneEighths = 1.0f; // fallback: 1 eighth

        const FGASShotListSceneRow* FoundScene = SceneRows.FindByPredicate(
            [&Block](const FGASShotListSceneRow& Row)
            {
                return Row.SceneId == Block.Id;
            }
        );

        if (FoundScene && FoundScene->LengthEighths > 0)
        {
            SceneEighths = (float)FoundScene->LengthEighths;
        }

        const float SceneSeconds =
            SceneEighths * SecondsPerEighth;

        const int32 SceneFrames =
            FMath::RoundToInt(
                SceneSeconds * FPS
            );

        FMovieSceneMarkedFrame Marker;

        Marker.FrameNumber =
            MovieScene->GetTickResolution()
            .AsFrameNumber(
                CurrentFrame / FPS
            );

        Marker.Label =
            FString::Printf(
                TEXT("%s"),
                *Block.Text
            );

        MovieScene->AddMarkedFrame(Marker);

        UE_LOG(LogTemp, Warning,
            TEXT("SCENE MARKER: %s at frame %d  (%.1f eighths)"),
            *Marker.Label,
            CurrentFrame,
            SceneEighths
        );

        CurrentFrame += SceneFrames;
    }

    // ------------------------------------------------------------
    // FINALIZE
    // ------------------------------------------------------------

    MovieScene->SetPlaybackRange(
        0,
        CurrentFrame
    );

    MovieScene->SetWorkingRange(
        0.0,
        (double)CurrentFrame / FPS
    );

    MovieScene->SetViewRange(
        0.0,
        (double)CurrentFrame / FPS
    );

    MovieScene->MarkAsChanged();

    FPropertyChangedEvent DummyEvent(nullptr);

    MovieScene->PostEditChangeProperty(
        DummyEvent
    );

    Sequence->MarkPackageDirty();

    SaveSequenceAsset(Sequence);

    UE_LOG(LogTemp, Warning,
        TEXT("Scene markers generated."));
}

void SGAS_ScriptTab::SyncSequencerToScene(const FString& SceneMarkerLabel)
{
    if (SceneMarkerLabel.IsEmpty() || !ActiveProject)
        return;

    const FString MasterPackagePath =
        FString::Printf(
            TEXT("/Game/PrePro/Projects/%s/Sequences/MASTER_%s"),
            *ActiveProject->ProjectName,
            *ActiveProject->ProjectName
        );

    ULevelSequence* MasterSequence =
        LoadObject<ULevelSequence>(nullptr, *MasterPackagePath);

    if (!MasterSequence)
    {
        UE_LOG(LogTemp, Error, TEXT("SyncSequencerToScene: Could not load MASTER sequence at %s"), *MasterPackagePath);
        return;
    }

    UMovieScene* MovieScene = MasterSequence->GetMovieScene();
    if (!MovieScene)
        return;

    // Log all markers so we can see what's there
    for (const FMovieSceneMarkedFrame& Marker : MovieScene->GetMarkedFrames())
    {
        UE_LOG(LogTemp, Warning, TEXT("MARKER: '%s' at frame %d"), *Marker.Label, Marker.FrameNumber.Value);
    }

    for (const FMovieSceneMarkedFrame& Marker : MovieScene->GetMarkedFrames())
    {
        if (!Marker.Label.Equals(SceneMarkerLabel, ESearchCase::IgnoreCase))
            continue;

        UE_LOG(LogTemp, Warning, TEXT("SyncSequencerToScene: FOUND '%s' at frame %d"), *SceneMarkerLabel, Marker.FrameNumber.Value);

        // Get sequencer via ISequencerModule
        ISequencerModule& SequencerModule =
            FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");

        TSharedPtr<ISequencer> Sequencer;

        // Open the master sequence first
        ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence(MasterSequence);

        IAssetEditorInstance* EditorInstance =
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
            ->FindEditorForAsset(MasterSequence, true);

        if (!EditorInstance)
        {
            UE_LOG(LogTemp, Error, TEXT("SyncSequencerToScene: No editor instance"));
            return;
        }

        ILevelSequenceEditorToolkit* Toolkit =
            static_cast<ILevelSequenceEditorToolkit*>(EditorInstance);

        if (!Toolkit)
        {
            UE_LOG(LogTemp, Error, TEXT("SyncSequencerToScene: Toolkit cast failed"));
            return;
        }

        Sequencer = Toolkit->GetSequencer();

        if (!Sequencer.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("SyncSequencerToScene: Sequencer invalid"));
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("SyncSequencerToScene: Got sequencer, jumping to frame %d"), Marker.FrameNumber.Value);

        UE_LOG(LogTemp, Warning, TEXT("SyncSequencerToScene: Looking for '%s'"), *SceneMarkerLabel);

        const FFrameTime TargetTime =
            FFrameRate::TransformTime(
                FFrameTime(Marker.FrameNumber),
                MovieScene->GetTickResolution(),
                Sequencer->GetFocusedTickResolution()
            );

        Sequencer->SetGlobalTime(TargetTime);
        Sequencer->ForceEvaluate();

        return;
    }

    UE_LOG(LogTemp, Error, TEXT("SyncSequencerToScene: No marker found matching '%s'"), *SceneMarkerLabel);
}
