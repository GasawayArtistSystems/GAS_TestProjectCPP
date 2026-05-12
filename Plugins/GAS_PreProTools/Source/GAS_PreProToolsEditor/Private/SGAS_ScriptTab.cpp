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
#include "ISequencerModule.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"

ACineCameraActor* GLastCreatedCamera = nullptr;


// =======================================================
// Fixed panel widths (UI constants)
// =======================================================
static constexpr float ScriptPanelWidth = 810.f;
static constexpr float ShotListPanelWidth = 910.f;

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

void SGAS_ScriptTab::OnEnableBlockingChanged(ECheckBoxState NewState)
{
    PendingProjectSettings.bEnableBlocking =
        (NewState == ECheckBoxState::Checked);
}

void SGAS_ScriptTab::OnEnableShotLayersChanged(ECheckBoxState NewState)
{
    PendingProjectSettings.bEnableShotLayers =
        (NewState == ECheckBoxState::Checked);
}

void SGAS_ScriptTab::OnAutoMasterSequenceChanged(ECheckBoxState NewState)
{
    PendingProjectSettings.bAutoCreateMasterSequence =
        (NewState == ECheckBoxState::Checked);
}

// =======================================================
// FOR BLOCKING SETUP
// =======================================================

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
    TWeakObjectPtr<ACineCameraActor> CameraPtr = nullptr;

    if (FGASShotIntent* Intent = CurrentScript.ShotIntents.Find(MarkerId))
    {
        CameraPtr = Intent->CameraActor.Get();
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
                if (ACineCameraActor* Camera = CameraPtr.Get())
                {
                    Camera->SetActorHiddenInGame(true);
                    Camera->SetIsTemporarilyHiddenInEditor(true);

                    UE_LOG(LogGASPrePro, Warning,
                        TEXT("[DeleteShot] Camera hidden instead of destroyed: %s"),
                        *Camera->GetName());
                }
            }
            else
            {
                UE_LOG(LogGASPrePro, Warning,
                    TEXT("[DeleteShot] Camera pointer invalid, skipping actor destroy"));
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
    if (!Script.IsValid())
    {
        return;
    }

    FGASShotIntent* Shot = Script->ShotIntents.Find(MarkerId);
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

    UE_LOG(LogTemp, Warning, TEXT("Camera Saved: %s"), *Location.ToString());

    // ------------------------------------------------------------
    // APPLY TO REAL CAMERA (CRITICAL FIX)
    // ------------------------------------------------------------
    if (Shot->CameraActor.IsValid())
    {
        ACineCameraActor* Cam = Shot->CameraActor.Get();
        if (Cam)
        {
            Cam->SetActorLocationAndRotation(Location, Rotation);

            if (UCineCameraComponent* Cine = Cam->GetCineCameraComponent())
            {
                Cine->SetCurrentFocalLength(FocalLength);
            }

            UE_LOG(LogTemp, Warning, TEXT("Applied to REAL camera"));
        }
    }
}

void SGAS_ScriptTab::InitializeCameraPreview(ACineCameraActor* InSourceCamera)
{
    static double LastPreviewInitTime = 0.0;

    const double CurrentTime = FPlatformTime::Seconds();

    if ((CurrentTime - LastPreviewInitTime) < 0.25)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("InitializeCameraPreview THROTTLED"));
        return;
    }

    LastPreviewInitTime = CurrentTime;


    if (GExitPurge || GIsRequestingExit || IsEngineExitRequested())
    {
        return;
    }

    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!EditorWorld || !IsValid(EditorWorld))
    {
        return;
    }

    int32 SceneCaptureCount = 0;
    int32 CineCameraCount = 0;

    for (TObjectIterator<USceneCaptureComponent2D> It; It; ++It)
    {
        ++SceneCaptureCount;
    }

    for (TObjectIterator<ACineCameraActor> It; It; ++It)
    {
        ++CineCameraCount;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("PREVIEW AUDIT | SceneCaptures=%d | CineCameras=%d"),
        SceneCaptureCount,
        CineCameraCount);

    UE_LOG(LogTemp, Warning, TEXT("InitializeCameraPreview called"));

    if (!GEditor)
    {
        return;
    }

    if (!IsValid(InSourceCamera) || !InSourceCamera->IsValidLowLevel())
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeCameraPreview: invalid camera"));
        return;
    }

    if (!InSourceCamera->GetWorld() || InSourceCamera->GetWorld()->WorldType != EWorldType::Editor)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeCameraPreview: camera is not in editor world"));
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World || World->WorldType != EWorldType::Editor)
    {
        UE_LOG(LogTemp, Error, TEXT("INVALID WORLD CONTEXT"));
        return;
    }

    if (!InSourceCamera || !InSourceCamera->GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("Camera has no valid world"));
        return;
    }

    if (InSourceCamera->GetWorld() != World)
    {
        UE_LOG(LogTemp, Warning, TEXT("Camera belongs to a different world — skipping preview init"));
        return;
    }

    PreviewSourceCamera = InSourceCamera;

    // --------------------------------------------------
    // If preview component belongs to an old camera / old world, kill it
    // --------------------------------------------------

    if (!InSourceCamera ||
        !IsValid(InSourceCamera) ||
        InSourceCamera->IsActorBeingDestroyed())
    {
        UE_LOG(LogTemp, Error,
            TEXT("InitializeCameraPreview: source camera invalid before assignment"));
        return;
    }

    UCineCameraComponent* SourceCameraComponent =
        InSourceCamera->GetCineCameraComponent();

    if (!SourceCameraComponent ||
        !IsValid(SourceCameraComponent) ||
        SourceCameraComponent->IsBeingDestroyed())
    {
        UE_LOG(LogTemp, Error,
            TEXT("InitializeCameraPreview: source camera component invalid"));
        return;
    }

    PreviewCameraActor = InSourceCamera;
    PreviewCameraComponent = SourceCameraComponent;

    // --------------------------------------------------
    // Aspect from PROJECT (authoritative)
    // --------------------------------------------------
    FString Aspect = TEXT("16:9");

    if (ActiveProject)
    {
        Aspect = ActiveProject->ProjectSettings.AspectRatio;
    }

    const FVector2D Ratio = GAS_ParseAspectRatio(Aspect);

    const float AspectRatio =
        (Ratio.Y > KINDA_SMALL_NUMBER)
        ? (Ratio.X / Ratio.Y)
        : (16.0f / 9.0f);

    const int32 TargetWidth = 1280;
    const int32 TargetHeight = FMath::RoundToInt((float)TargetWidth / AspectRatio);

    UE_LOG(LogTemp, Warning, TEXT("Preview Aspect: %s  →  %f"), *Aspect, AspectRatio);

    // --------------------------------------------------
    // Render Target
    // --------------------------------------------------
    if (!CameraPreviewRenderTarget)
    {
        CameraPreviewRenderTarget =
            NewObject<UTextureRenderTarget2D>(GetTransientPackage());

        CameraPreviewRenderTarget->ClearColor = FLinearColor::Black;
        CameraPreviewRenderTarget->InitAutoFormat(TargetWidth, TargetHeight);
        //CameraPreviewRenderTarget->UpdateResourceImmediate(true);

        UE_LOG(LogTemp, Warning, TEXT("RenderTarget CREATED %d x %d"), TargetWidth, TargetHeight);
    }
    else if (CameraPreviewRenderTarget->SizeX != TargetWidth ||
        CameraPreviewRenderTarget->SizeY != TargetHeight)
    {
        CameraPreviewRenderTarget->ResizeTarget(TargetWidth, TargetHeight);
        //CameraPreviewRenderTarget->UpdateResourceImmediate(true);

        UE_LOG(LogTemp, Warning, TEXT("RenderTarget RESIZED %d x %d"), TargetWidth, TargetHeight);
    }

    // --------------------------------------------------
    // Reuse existing scene capture whenever possible
    // --------------------------------------------------

    const bool bNeedNewSceneCapture =
        !PreviewSceneCapture ||
        !IsValid(PreviewSceneCapture) ||
        PreviewSceneCapture->IsBeingDestroyed() ||
        PreviewSceneCapture->GetWorld() != World;

    if (bNeedNewSceneCapture)
    {
        if (PreviewSceneCapture &&
            IsValid(PreviewSceneCapture) &&
            !PreviewSceneCapture->IsBeingDestroyed())
        {
            PreviewSceneCapture->DestroyComponent();
        }

        PreviewSceneCapture = nullptr;

        PreviewSceneCapture = NewObject<USceneCaptureComponent2D>(
            InSourceCamera,
            USceneCaptureComponent2D::StaticClass(),
            NAME_None,
            RF_Transient
        );

        if (!PreviewSceneCapture)
        {
            UE_LOG(LogTemp, Error,
                TEXT("Failed to create SceneCapture"));
            return;
        }

        PreviewSceneCapture->SetupAttachment(
            PreviewCameraActor->GetRootComponent());

        PreviewSceneCapture->RegisterComponentWithWorld(World);

        UE_LOG(LogTemp, Warning,
            TEXT("NEW PreviewSceneCapture CREATED"));
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("REUSING existing PreviewSceneCapture"));
    }

    // --------------------------------------------------
    // FORCE WORLD MATCH (THIS IS THE FIX)
    // --------------------------------------------------

    PreviewSceneCapture->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

    PreviewSceneCapture->SetWorldLocation(
        PreviewCameraActor->GetActorLocation()
    );

    PreviewSceneCapture->SetWorldRotation(
        PreviewCameraActor->GetActorRotation()
    );

    // --------------------------------------------------
    // Hard match real camera
    // --------------------------------------------------
    PreviewSceneCapture->TextureTarget = CameraPreviewRenderTarget;
    PreviewSceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

    // ADD THIS BLOCK HERE
    PreviewSceneCapture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    PreviewSceneCapture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;

    PreviewSceneCapture->PostProcessSettings.bOverride_AutoExposureBias = true;
    PreviewSceneCapture->PostProcessSettings.AutoExposureBias = -2.0f;

    PreviewSceneCapture->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
    PreviewSceneCapture->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
    PreviewSceneCapture->PostProcessSettings.AutoExposureMinBrightness = 1.0f;
    PreviewSceneCapture->PostProcessSettings.AutoExposureMaxBrightness = 1.0f;

    PreviewSceneCapture->ProjectionType = ECameraProjectionMode::Perspective;
    PreviewSceneCapture->FOVAngle =
        PreviewCameraComponent->CurrentFocalLength > 0.f
        ? PreviewCameraComponent->GetHorizontalFieldOfView()
        : 50.f;
    // Match clip planes (prevents weird horizon clipping)
    PreviewSceneCapture->MaxViewDistanceOverride = 100000.f;
    PreviewSceneCapture->bUseCustomProjectionMatrix = false;

    PreviewSceneCapture->PostProcessSettings = PreviewCameraComponent->PostProcessSettings;
    PreviewSceneCapture->PostProcessBlendWeight = PreviewCameraComponent->PostProcessBlendWeight;

    PreviewSceneCapture->PrimitiveRenderMode =
        ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;

    PreviewSceneCapture->ShowFlags = FEngineShowFlags(ESFIM_Game);
    PreviewSceneCapture->ShowFlags.SetModeWidgets(false);
    PreviewSceneCapture->ShowFlags.SetSelection(false);
    PreviewSceneCapture->ShowFlags.SetSelectionOutline(false);
    PreviewSceneCapture->ShowFlags.SetGrid(false);
    PreviewSceneCapture->ShowFlags.SetBounds(false);
    PreviewSceneCapture->ShowFlags.SetPivot(false);

    PreviewSceneCapture->bCaptureEveryFrame = false;
    PreviewSceneCapture->bCaptureOnMovement = false;

    // --------------------------------------------------
    // FORCE MATCH REAL CAMERA (CRITICAL FIX)
    // --------------------------------------------------

    PreviewSceneCapture->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

    PreviewSceneCapture->SetWorldLocation(
        InSourceCamera->GetActorLocation()
    );

    PreviewSceneCapture->SetWorldRotation(
        InSourceCamera->GetActorRotation()
    );

    // --------------------------------------------------
    // Final capture
    // --------------------------------------------------

    /*PreviewSceneCapture->CaptureScene();*/

    UE_LOG(LogTemp, Warning, TEXT("Preview synced AFTER camera transform"));
    UE_LOG(LogTemp, Warning, TEXT("REAL CAM LOC: %s"),
        *InSourceCamera->GetActorLocation().ToString());

    UE_LOG(LogTemp, Warning, TEXT("CAPTURE LOC: %s"),
        *PreviewSceneCapture->GetComponentLocation().ToString());

    UE_LOG(LogTemp, Warning, TEXT("Preview matched to REAL camera"));
    UE_LOG(LogTemp, Warning, TEXT("AspectRatio: %f"), AspectRatio);
    UE_LOG(LogTemp, Warning, TEXT("FOV: %f"), PreviewCameraComponent->FieldOfView);
}

void SGAS_ScriptTab::UpdateCameraFromShotIntent(FGASShotIntent& Intent)
{
    if (!GEditor) return;

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return;

    // --------------------------------------------------
    // Resolve camera
    // --------------------------------------------------
    ACineCameraActor* Cam = Intent.CameraActor.Get();
    if (!Cam) return;

    if (!IsValid(Cam))
    {
        UE_LOG(LogTemp, Error, TEXT("[GAS CameraPreview] Invalid camera actor. Skipping preview update."));
        return;
    }

    if (Cam->GetWorld() != World)
    {
        UE_LOG(LogTemp, Error, TEXT("[GAS CameraPreview] Camera belongs to different world. Skipping preview update."));
        return;
    }

    if (Intent.SubjectId == TEXT("(No Subject)"))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GAS CameraPreview] No Subject selected. Skipping camera framing."));

        return;
    }

    // --------------------------------------------------
    // 🔥 USE SAVED CAMERA IF AVAILABLE (THIS IS THE FIX)
    // --------------------------------------------------
    if (bRestoreSavedCameraThisFrame)
    {
        Cam->SetActorLocationAndRotation(
            Intent.CameraLocation,
            Intent.CameraRotation
        );

        if (UCineCameraComponent* Cine = Cam->GetCineCameraComponent())
        {
            Cine->SetCurrentFocalLength(Intent.CameraFocalLength);
        }

        UE_LOG(LogTemp, Warning, TEXT("APPLIED FROM SHOT INTENT: %s"),
            *Intent.CameraLocation.ToString());

        bRestoreSavedCameraThisFrame = false;
    }
    else
    {
        // --------------------------------------------------
        // FALLBACK (ONLY if no saved camera yet)
        // --------------------------------------------------
        AGAS_StandInActor* TargetActor = nullptr;

        for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
        {
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
            UArrowComponent* Eyeline = TargetActor->FindComponentByClass<UArrowComponent>();

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

        FVector CameraLocation =
            TargetLocation
            + (CameraDirection * Distance)
            + FVector(0.f, 0.f, HeightOffset);

        FVector LookAtTarget = TargetLocation + FVector(0.f, 0.f, HeightOffset);

        FRotator LookAtRotation =
            (LookAtTarget - CameraLocation).Rotation();

        Cam->SetActorLocation(CameraLocation);
        Cam->SetActorRotation(LookAtRotation);

        UE_LOG(LogTemp, Warning, TEXT("FALLBACK CAMERA USED"));
    }

    // --------------------------------------------------
    // Preview handled by SGAS_ShotIntentWindow only.
    // Do NOT initialize SGAS_ScriptTab preview here.
    // This prevents stale cross-world SceneCapture crashes.
    // --------------------------------------------------
    return;
}

void SGAS_ScriptTab::SyncPreviewToRealCamera()
{
    ACineCameraActor* SourceCamera = PreviewSourceCamera.Get();
    if (!SourceCamera || !PreviewCaptureComponent || !CameraPreviewRenderTarget)
    {
        return;
    }

    UCineCameraComponent* SourceCine =
        SourceCamera->GetCineCameraComponent();

    if (!SourceCine)
    {
        return;
    }

    // --------------------------------------------------
    // FORCE ASPECT RATIO (FROM PROJECT SETTING)
    // --------------------------------------------------
    float AspectRatio = 16.0f / 9.0f;

    if (ActiveProject)
    {
        const FString& RatioString = ActiveProject->ProjectSettings.AspectRatio;

        if (RatioString.Contains(TEXT(":")))
        {
            FString Left, Right;
            if (RatioString.Split(TEXT(":"), &Left, &Right))
            {
                AspectRatio = FCString::Atof(*Left) / FCString::Atof(*Right);
            }
        }
    }

    // Apply to filmback
    if (SourceCine->Filmback.SensorHeight > 0.f)
    {
        SourceCine->Filmback.SensorWidth =
            SourceCine->Filmback.SensorHeight * AspectRatio;
    }

    PreviewCaptureComponent->SetWorldLocation(SourceCamera->GetActorLocation());
    PreviewCaptureComponent->SetWorldRotation(SourceCamera->GetActorRotation());

    PreviewCaptureComponent->FOVAngle = SourceCine->FieldOfView;
    PreviewCaptureComponent->OrthoWidth = 0.0f;
    PreviewCaptureComponent->TextureTarget = CameraPreviewRenderTarget;
    PreviewCaptureComponent->bUseCustomProjectionMatrix = false;

    PreviewCaptureComponent->PostProcessSettings = SourceCine->PostProcessSettings;
    PreviewCaptureComponent->PostProcessBlendWeight = SourceCine->PostProcessBlendWeight;

    PreviewCaptureComponent->CaptureScene();
}


void SGAS_ScriptTab::ReleaseCameraPreview()
{
    UE_LOG(LogTemp, Warning,
        TEXT("ReleaseCameraPreview CALLED"));

    const bool bEngineShuttingDown =
        GExitPurge ||
        GIsRequestingExit ||
        IsEngineExitRequested();

    // --------------------------------------------------
    // PreviewSceneCapture
    // --------------------------------------------------

    if (PreviewSceneCapture)
    {
        if (IsValid(PreviewSceneCapture))
        {
            PreviewSceneCapture->TextureTarget = nullptr;
            PreviewSceneCapture->bCaptureEveryFrame = false;
            PreviewSceneCapture->bCaptureOnMovement = false;

            if (!bEngineShuttingDown &&
                !PreviewSceneCapture->IsBeingDestroyed())
            {
                PreviewSceneCapture->DestroyComponent();
            }
        }

        PreviewSceneCapture = nullptr;
    }

    // --------------------------------------------------
    // PreviewCaptureComponent
    // --------------------------------------------------

    if (PreviewCaptureComponent)
    {
        if (IsValid(PreviewCaptureComponent))
        {
            PreviewCaptureComponent->TextureTarget = nullptr;
            PreviewCaptureComponent->bCaptureEveryFrame = false;
            PreviewCaptureComponent->bCaptureOnMovement = false;

            if (!bEngineShuttingDown &&
                !PreviewCaptureComponent->IsBeingDestroyed())
            {
                PreviewCaptureComponent->DestroyComponent();
            }
        }

        PreviewCaptureComponent = nullptr;
    }

    // --------------------------------------------------
    // Preview Camera Actor
    // --------------------------------------------------

    if (PreviewCameraActor)
    {
        if (IsValid(PreviewCameraActor))
        {
            if (!bEngineShuttingDown &&
                !PreviewCameraActor->IsActorBeingDestroyed())
            {
                PreviewCameraActor->Destroy();
            }
        }

        PreviewCameraActor = nullptr;
    }

    // --------------------------------------------------
    // Remaining refs
    // --------------------------------------------------

    PreviewSourceCamera = nullptr;
    PreviewCameraComponent = nullptr;

    if (CameraPreviewRenderTarget &&
        IsValid(CameraPreviewRenderTarget))
    {
        CameraPreviewRenderTarget = nullptr;
    }

    PreviewBrush.Reset();

    UE_LOG(LogTemp, Warning,
        TEXT("ReleaseCameraPreview FINISHED"));
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
                // LEFT PANEL — FIXED WIDTH (NO RESIZING)
                // =======================================================
                +SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                        .WidthOverride(52.f)
                        .MinDesiredWidth(52.f)
                        .MaxDesiredWidth(52.f)
                        [
                            SNew(SVerticalBox)

                                // --- Shot Marking Button --------------------------------
                                + SVerticalBox::Slot()
                                .AutoHeight()
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

                            // --- Scene Number Toggle --------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
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
                                                .ToolTipText(FText::FromString("Scene Number"))
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SceneNumberIcon"))
                                                        .ColorAndOpacity_Lambda([this]()
                                                            {
                                                                return bShowSceneNumbers
                                                                    ? FLinearColor(0.3f, 1.f, 0.3f, 1.f) // green = active
                                                                    : FLinearColor::White;
                                                            })
                                                ]
                                        ]
                                ]

                            // --- Save Script --------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)
                                                .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnSaveScript))
                                                .ToolTipText(FText::FromString("Save Script"))
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.SaveIcon"))
                                                ]
                                        ]

                                ]
                            // --- Divider --------------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 12, 0, 12)
                                [
                                    SNew(SBorder)
                                        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.f))
                                        .Padding(FMargin(0.f, 1.f))
                                ]

                                // --- CAST BUTTON -----------------------------------------
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(4.f)
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)
                                                .ToolTipText(FText::FromString(TEXT("Edit Scene Cast")))
                                                .IsEnabled(true)
                                                .OnClicked_Lambda([this]()
                                                    {
                                                        if (!bBlockingActive)
                                                        {
                                                            UE_LOG(LogGASPrePro, Warning, TEXT("Blocking not active — window denied"));
                                                            return FReply::Handled();
                                                        }

                                                        // -----------------------------------------------------
                                                        // Resolve Scene Title
                                                        // -----------------------------------------------------
                                                        FString SceneTitle = TEXT("Unknown Scene");

                                                        for (const FGASBlock& Block : CurrentScript.Blocks)
                                                        {
                                                            if (Block.Id == ActiveBlockingSceneId.ToString())
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
                                                            .ClientSize(FVector2D(600.f, 400.f));

                                                        Window->SetContent(
                                                            SNew(SGAS_BlockingCastWindow)
                                                            .SceneId(ActiveBlockingSceneId)
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
                                                                UE_LOG(LogGASPrePro, Warning, TEXT("Cast window closed (blocking still active)"));
                                                            }
                                                        ));

                                                        FSlateApplication::Get().AddWindow(Window);

                                                        return FReply::Handled();
                                                    })
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.CastIcon"))
                                                        .ColorAndOpacity(FLinearColor::White)
                                                ]
                                        ]
                                ]

                            // --- Divider --------------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 12, 0, 12)
                                [
                                    SNew(SBorder)
                                        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.f))
                                        .Padding(FMargin(0.f, 1.f))
                                ]

                            // --- Edit Mode Toggle -----------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)

                                                .IsEnabled_Lambda([this]()
                                                    {
                                                        return CurrentScript.Blocks.Num() > 0;
                                                    })

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
                                                                    ? FLinearColor(0.2f, 0.9f, 0.2f, 1.f)   // green when editing
                                                                    : FLinearColor::White;                // normal when not
                                                            })
                                                ]
                                        ]
                                ]

                            // --- Add Mode Toggle -----------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SBox)
                                        .WidthOverride(48.f)
                                        .HeightOverride(48.f)
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                                .HAlign(HAlign_Center)
                                                .VAlign(VAlign_Center)

                                                .IsEnabled_Lambda([this]()
                                                    {
                                                        return CurrentScript.Blocks.Num() > 0;
                                                    })

                                                .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnToggleAddMode))
                                                .ToolTipText_Lambda([this]()
                                                    {
                                                        return bIsAddMode
                                                            ? FText::FromString("Exit Add Mode")
                                                            : FText::FromString("Enter Add Mode");
                                                    })
                                                [
                                                    SNew(SImage)
                                                        .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.EditAddIcon")) // you’ll add this icon
                                                        .ColorAndOpacity_Lambda([this]()
                                                            {
                                                                return bIsAddMode
                                                                    ? FLinearColor(0.2f, 0.6f, 1.0f, 1.f)  // blue when adding
                                                                    : FLinearColor::White;
                                                            })
                                                ]
                                        ]
                                ]



                            // --- Divider --------------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 12, 0, 12)
                                [
                                    SNew(SBorder)
                                        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.f))
                                        .Padding(FMargin(0.f, 1.f))
                                ]


                            // --- Import Script --------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 8, 0, 0)
                                [
                                    SNew(SButton)
                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                        .HAlign(HAlign_Center)
                                        .VAlign(VAlign_Center)
                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnImportScript))
                                        .ToolTipText(FText::FromString("Import Script"))
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.ImportScript"))
                                        ]

                                ]

                            // --- Generate Page Breaks -------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 8, 0, 0)
                                [
                                    SNew(SButton)
                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                        .HAlign(HAlign_Center)
                                        .VAlign(VAlign_Center)

                                        // Enabled ONLY when no page breaks exist
                                        .IsEnabled_Lambda([this]()
                                            {
                                                return CurrentScript.Blocks.Num() > 0
                                                    && CurrentScript.UserPageBreaks.Num() == 0;
                                            })

                                        // Tooltip explains state
                                        .ToolTipText_Lambda([this]()
                                            {
                                                if (CurrentScript.Blocks.Num() == 0)
                                                {
                                                    return FText::FromString("No script loaded.");
                                                }

                                                if (CurrentScript.UserPageBreaks.Num() == 0)
                                                {
                                                    return FText::FromString(
                                                        "Generate approximate page breaks.\n"
                                                        "Page breaks can be edited afterward."
                                                    );
                                                }
                                                else
                                                {
                                                    return FText::FromString(
                                                        "Page breaks already exist.\n"
                                                        "Clear them to regenerate."
                                                    );
                                                }
                                            })




                                        .OnClicked(FOnClicked::CreateSP(
                                            this,
                                            &SGAS_ScriptTab::OnGeneratePageBreaks
                                        ))
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.PageBreakIcon"))
                                        ]

                                ]

                            
                            // --- Clear Script ---------------------------------------
                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 8, 0, 0)
                                [
                                    SNew(SButton)
                                        .ButtonStyle(&FGAS_PreProToolsStyle::Get().GetWidgetStyle<FButtonStyle>("GAS.ToolButton"))
                                        .HAlign(HAlign_Center)
                                        .VAlign(VAlign_Center)
                                        .OnClicked(FOnClicked::CreateSP(this, &SGAS_ScriptTab::OnClearScriptConfirm))
                                        .ToolTipText(FText::FromString("Clear Script"))
                                        [
                                            SNew(SImage)
                                                .Image(FGAS_PreProToolsStyle::Get().GetBrush("GAS.ClearIcon"))
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

        RebuildShotList();
        RebuildCastUI();

        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateSP(
                this,
                &SGAS_ScriptTab::TickShotModeResume
            )
        );

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

    if (GActiveBlockingSequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("Saving Sequence"));
        SaveSequenceAsset(GActiveBlockingSequence);
    }

#if WITH_EDITOR
    if (GEditor)
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();

        if (World)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Marking LEVEL dirty only"));

            World->MarkPackageDirty();
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
    if (PreviewSourceCamera.IsValid())
    {
        ReleaseCameraPreview();
    }

    PreviewSourceCamera.Reset();

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
            .AutoHeight()
            .Padding(10.f, 10.f, 10.f, 8.f)
            [
                SNew(SBorder)
                    .Padding(FMargin(8.f, 6.f))
                    .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("WORKFLOW")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    ]
            ]

        + SVerticalBox::Slot()
            .Padding(10.f, 2.f, 10.f, 10.f)
            .AutoHeight()
            [
                SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 2.f)
                    [
                        SNew(SCheckBox)
                            .IsChecked(ECheckBoxState::Checked)
                            .OnCheckStateChanged(
                                this,
                                &SGAS_ScriptTab::OnEnableBlockingChanged
                            )
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Enable Blocking")))
                            ]
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 2.f)
                    [
                        SNew(SCheckBox)
                            .IsChecked(ECheckBoxState::Checked)
                            .OnCheckStateChanged(
                                this,
                                &SGAS_ScriptTab::OnEnableShotLayersChanged
                            )
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Enable Shot Layers")))
                            ]
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 2.f)
                    [
                        SNew(SCheckBox)
                            .IsChecked(ECheckBoxState::Checked)
                            .OnCheckStateChanged(
                                this,
                                &SGAS_ScriptTab::OnAutoMasterSequenceChanged
                            )
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Auto Create Master Sequence")))
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

    NewProject->ProjectSettings.bEnableBlocking =
        InProjectSettings.bEnableBlocking;

    NewProject->ProjectSettings.bEnableShotLayers =
        InProjectSettings.bEnableShotLayers;

    NewProject->ProjectSettings.bAutoCreateMasterSequence =
        InProjectSettings.bAutoCreateMasterSequence;

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
                    .OnCheckStateChanged_Lambda([&Member](ECheckBoxState State)
                        {
                            Member.bEnabled = (State == ECheckBoxState::Checked);
                        })
                    [
                        SNew(STextBlock)
                            .Text_Lambda([&Member]()
                                {
                                    return FText::FromString(Member.Name);
                                })
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
    FGASBlock& SceneBlock
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
        }

        return Seq;
    }


    // ---------------------------------------------------
    // Create new Level Sequence asset
    // ---------------------------------------------------

    FString AssetName = SceneBlock.BlockingLevelPath;

    // Extract the map name from the path
    AssetName = FPackageName::GetShortName(AssetName);

    // Convert "_BLOCKING" → "_Master"
    AssetName = AssetName.Replace(TEXT("_BLOCKING"), TEXT("_Master"));

    // Derive project root from blocking level path
    FString BlockingPath = SceneBlock.BlockingLevelPath;

    // Strip everything after "/Blocking"
    int32 BlockingIndex = INDEX_NONE;
    if (BlockingPath.FindChar('/', BlockingIndex))
    {
        BlockingIndex = BlockingPath.Find(TEXT("/Blocking"));
    }

    FString ProjectRoot = BlockingPath.Left(BlockingPath.Find(TEXT("/Blocking")));

    // Final sequence folder
    FString PackagePath = ProjectRoot / TEXT("Sequences");

    const FString FullPath =
        PackagePath / AssetName;

    UPackage* Package = CreatePackage(*FullPath);

    ULevelSequence* NewSequence =
        NewObject<ULevelSequence>(
            Package,
            ULevelSequence::StaticClass(),
            *AssetName,
            RF_Public | RF_Standalone
        );

    // Ensure MovieScene exists
    NewSequence->Initialize();

    FAssetRegistryModule::AssetCreated(NewSequence);
    Package->MarkPackageDirty();

    // Save
    const FString Filename =
        FPackageName::LongPackageNameToFilename(
            FullPath,
            FPackageName::GetAssetPackageExtension()
        );

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;

    UPackage::SavePackage(
        Package,
        NewSequence,
        *Filename,
        SaveArgs
    );

    SceneBlock.MasterSequencePath = FullPath + TEXT(".") + AssetName;

    return NewSequence;
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

    StopBlocking();
    bPendingAutoOpenCastWindow = true;

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

    if (!SceneBlock->BlockingLevelPath.IsEmpty())
    {
        // Load blocking map
        FEditorFileUtils::LoadMap(SceneBlock->BlockingLevelPath, false, true);

        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

        if (GActiveBlockingSequence)
        {
            GActiveBlockingSequence = nullptr;
        }

        // -----------------------------------------------------
        // Spawn characters for this scene
        // -----------------------------------------------------

        if (World)
        {
            for (FString CharacterName : SceneBlock->CharactersInScene)
            {
                CharacterName = CharacterName.ToUpper();

                AGAS_StandInActor* ExistingActor = nullptr;

                for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
                {
                    AGAS_StandInActor* Actor = *It;

                    if (Actor && Actor->GAS_CharacterId == CharacterName)
                    {
                        ExistingActor = Actor;
                        break;
                    }
                }

                if (!ExistingActor)
                {
                    FVector SpawnLocation = FVector::ZeroVector;

                    FActorSpawnParameters Params;
                    Params.OverrideLevel = World->PersistentLevel;
                    Params.SpawnCollisionHandlingOverride =
                        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                    ExistingActor =
                        World->SpawnActor<AGAS_StandInActor>(
                            AGAS_StandInActor::StaticClass(),
                            SpawnLocation,
                            FRotator::ZeroRotator,
                            Params
                        );

                    if (ExistingActor)
                    {
                        ExistingActor->GAS_CharacterId = CharacterName;
                        ExistingActor->SetActorLabel(CharacterName);
                    }
                }

                if (ExistingActor)
                {
                    ExistingActor->RefreshMeshFromScript(&CurrentScript);
                }
            }
        }

        // -----------------------------------------------------
        // Force mesh refresh for ALL stand-ins
        // -----------------------------------------------------

        if (World)
        {
            for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
            {
                AGAS_StandInActor* Actor = *It;

                if (!Actor)
                    continue;

                Actor->RefreshMeshFromScript(&CurrentScript);
            }
        }

        bBlockingActive = true;
        ActiveBlockingSceneId = FGuid(SceneId);

        GASBlockingAccess::SetBlockingActive(true);
        GASBlockingAccess::SetActiveSceneId(ActiveBlockingSceneId);

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

            UAssetEditorSubsystem* AssetEditorSubsystem =
                GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

            if (AssetEditorSubsystem)
            {
                AssetEditorSubsystem->OpenEditorForAsset(MasterSequence);
            }

            // -----------------------------------------------------
            // Rebuild Sequencer bindings
            // -----------------------------------------------------

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
        }

        // -----------------------------------------------------
        // 🔥 FINAL WORLD DUMP (AFTER EVERYTHING)
        // -----------------------------------------------------

        if (World)
        {
            World->GetTimerManager().SetTimerForNextTick([World]()
                {
                    UE_LOG(LogTemp, Error, TEXT("===== DELAYED WORLD DUMP ====="));

                    for (TActorIterator<AActor> It(World); It; ++It)
                    {
                        AActor* Actor = *It;
                        if (!Actor) continue;

                        for (UActorComponent* Comp : Actor->GetComponents())
                        {
                            if (USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Comp))
                            {
                                if (Skel->GetSkeletalMeshAsset())
                                {
                                    UE_LOG(LogTemp, Error,
                                        TEXT("🔥 FOUND MESH → Actor: %s | Class: %s | Mesh: %s"),
                                        *Actor->GetName(),
                                        *Actor->GetClass()->GetName(),
                                        *Skel->GetSkeletalMeshAsset()->GetName()
                                    );
                                }
                            }
                        }
                    }
                });
        }


        return;
    }

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
        NumberPart +
        TEXT("_") +
        HeadingPart +
        TEXT("_BLOCKING");

    const FString FullAssetPath = FolderPath + MapName;

    // ------------------------------------------------------------
    // Create new empty map properly
    // ------------------------------------------------------------
    UWorld* NewWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);

    if (!NewWorld)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Blocking: Failed to create blank map."));
        return false;
    }

    // Save it to desired path
    if (!UEditorLoadingAndSavingUtils::SaveMap(NewWorld, FullAssetPath))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Blocking: Failed to save blank map."));
        return false;
    }



    // Persist path
    SceneBlock.BlockingLevelPath = FullAssetPath;

    // Open map
    UEditorLoadingAndSavingUtils::LoadMap(FullAssetPath);

    // -------------------------------------------------
    // DELAY → ensure world is fully initialized
    // -------------------------------------------------
    FTimerHandle TimerHandle;

    GEditor->GetTimerManager()->SetTimer(
        TimerHandle,
        FTimerDelegate::CreateLambda([this, SceneBlock]()
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();

                if (!World)
                {
                    UE_LOG(LogGASPrePro, Error, TEXT("Blocking: World still null after load."));
                    return;
                }

                FGASStageLightingUtils::SetupDefaultStageLighting(World);

                UE_LOG(LogGASPrePro, Warning, TEXT("Blocking: Lighting initialized."));

                FinalizeBlockingLevel(World);

                // ------------------------------------------------------------
                // 🔥 FORCE SAVE AFTER SETUP
                // ------------------------------------------------------------
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

    // Activate blocking
    bBlockingActive = true;
    ActiveBlockingSceneId = FGuid(SceneBlock.Id);

    GASBlockingAccess::SetBlockingActive(true);
    GASBlockingAccess::SetActiveSceneId(ActiveBlockingSceneId);

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
    UE_LOG(LogTemp, Error, TEXT("🔥 OpenCastWindowForScene CALLED | SceneId=%s"), *SceneId);

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

            // Binding is handled after Sequencer opens (AssignSetToPendingScene -> BindAllStandInsToSequence)

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

    UE_LOG(LogGASPrePro, Warning, TEXT("[BLOCKING] Stopping blocking mode"));

    if (GActiveBlockingSequence)
    {
        SaveSequenceAsset(GActiveBlockingSequence);
    }

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

    // Persist script change
    MarkScriptDirty();
    SaveScriptToJson();

    // -----------------------------------------------------
    // Load blocking level
    // -----------------------------------------------------
    if (!bIsSaving)
    {
        UEditorLoadingAndSavingUtils::LoadMap(SceneBlock->BlockingLevelPath);
    }

    UE_LOG(LogGASPrePro, Warning, TEXT("BlockingLevelPath = %s"), *SceneBlock->BlockingLevelPath);

    UWorld* BlockingWorld = GEditor->GetEditorWorldContext().World();
    if (!BlockingWorld)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("AssignSetToPendingScene: BlockingWorld NULL"));
        return;
    }

    // -----------------------------------------------------
    // Duplicate set into blocking map
    // -----------------------------------------------------
    if (!DuplicateSetIntoBlockingLevel(Found->LevelPath, BlockingWorld))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("AssignSetToPendingScene: Duplication failed"));
        return;
    }

    //// -----------------------------------------------------
    //// Spawn cast BEFORE saving
    //// -----------------------------------------------------
    SpawnSceneCast(SceneBlock);

    // -----------------------------------------------------
    // Mark dirty + save
    // -----------------------------------------------------
    BlockingWorld->MarkPackageDirty();

    FString Filename = FPackageName::LongPackageNameToFilename(
        SceneBlock->BlockingLevelPath,
        FPackageName::GetMapPackageExtension()
    );

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    UPackage* Package = BlockingWorld->GetOutermost();


    UPackage::SavePackage(Package, BlockingWorld, *Filename, SaveArgs);

    UE_LOG(LogGASPrePro, Warning, TEXT("AssignSetToPendingScene: Set + Cast baked into blocking map"));

    // -----------------------------------------------------
    // Ensure Scene Master Sequence (Rehearsal Timeline)
    // -----------------------------------------------------
    FString SequenceName = SceneBlock->BlockingLevelPath;

    // Extract map name from path
    SequenceName = FPackageName::GetShortName(SequenceName);

    // Replace suffix
    SequenceName = SequenceName.Replace(TEXT("_BLOCKING"), TEXT("_Master"));

    ULevelSequence* MasterSequence =
        CreateOrLoadSceneMasterSequence(*SceneBlock);

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

    PendingBlockingSceneId.Empty();
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
    // 🔴 Reset toolbar shot state (controls icon tint)
    bShotAddArmed = false;

    // 🔴 Reset panel interaction state
    if (ScriptPanel.IsValid())
    {
        ScriptPanel->ResetEditorModes();
        ScriptPanel->SetShotAddArmed(false);
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
    // V2 SCENE HEADER ROW (STATIC)
    // =====================================================
    ShotListContainer->AddSlot()
        .AutoHeight()
        .Padding(2.f, 4.f)
        [
            SNew(SBorder)
                .Padding(4.f)
                .BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SSplitter)
                        .Orientation(Orient_Horizontal)

                        // Expand spacer (NO arrow in header)
                        + SSplitter::Slot()
                        .Value(0.06f)
                        [
                            SNew(STextBlock)
                                .Text(FText::GetEmpty())
                        ]

                        // PG
                        + SSplitter::Slot().Value(ColPage)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("PG")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Scene #
                        + SSplitter::Slot().Value(ColScene)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Scene")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Heading
                        + SSplitter::Slot().Value(ColHeading)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Scene Heading")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Length
                        + SSplitter::Slot().Value(ColLength)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("1/8")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Time
                        + SSplitter::Slot().Value(ColTime)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Time")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Set
                        + SSplitter::Slot().Value(ColSet)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Set")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]

                        // Notes
                        + SSplitter::Slot().Value(ColNotes)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Notes")))
                                .Font(FAppStyle::Get().GetFontStyle("BoldFont"))
                        ]
                ]
        ];

    if (SceneRowsV2.Num() == 0)
    {
        UE_LOG(LogGASPrePro, Verbose, TEXT("[ShotListV2] No scenes to render"));
        return;
    }

    for (const FGASShotListSceneRow& Scene : SceneRowsV2)
    {
        ShotListContainer->AddSlot()
            .AutoHeight()
            .Padding(4.f, 2.f)
            [
                SNew(SSplitter)
                    .Orientation(Orient_Horizontal)

                    // Expand arrow
                    + SSplitter::Slot()
                    .Value(0.06f)
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
                                    .Text_Lambda([this, Scene]()
                                        {
                                            return ExpandedScenes.Contains(Scene.SceneId)
                                                ? FText::FromString(TEXT("▼"))
                                                : FText::FromString(TEXT("▶"));
                                        })
                            ]
                    ]

                // PG
                + SSplitter::Slot().Value(ColPage)
                    [
                        SNew(STextBlock)
                            .Text(FText::AsNumber(Scene.StartPage))
                    ]

                    // Scene #
                    + SSplitter::Slot().Value(ColScene)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Scene.SceneNumber))
                    ]

                    // Scene Heading (clickable)
                    + SSplitter::Slot().Value(ColHeading)
                    [
                        SNew(SBorder)
                            .OnMouseButtonDown_Lambda(
                                [this, Scene](const FGeometry&, const FPointerEvent& MouseEvent)
                                {
                                    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
                                    {
                                        FMenuBuilder MenuBuilder(true, nullptr);

                                        // Find blocking path from script block
                                        FString BlockingPath;

                                        for (const FGASBlock& Block : CurrentScript.Blocks)
                                        {
                                            if (Block.Id == Scene.SceneId &&
                                                Block.Type == EGASBlockType::SceneHeading)
                                            {
                                                BlockingPath = Block.BlockingLevelPath;
                                                break;
                                            }
                                        }
                                        UE_LOG(LogGASPrePro, Warning, TEXT("Scene %s BlockingPath: %s"),
                                            *Scene.SceneId,
                                            *BlockingPath);
                                        const bool bHasBlocking = !BlockingPath.IsEmpty();

                                        if (!bHasBlocking)
                                        {
                                            MenuBuilder.AddMenuEntry(
                                                FText::FromString(TEXT("Start Blocking")),
                                                FText::FromString(TEXT("Create blocking level for this scene")),
                                                FSlateIcon(),
                                                FUIAction(
                                                    FExecuteAction::CreateLambda(
                                                        [this, SceneId = Scene.SceneId]()
                                                        {
                                                            this->OnStartBlockingScene(SceneId);
                                                        }
                                                    )
                                                )
                                            );
                                        }
                                        else
                                        {
                                            MenuBuilder.AddMenuEntry(
                                                FText::FromString(TEXT("Add Shots to Scene")),
                                                FText::FromString(TEXT("Enter shot marking mode for this scene")),
                                                FSlateIcon(),
                                                FUIAction(
                                                    FExecuteAction::CreateLambda(
                                                        [this, SceneId = Scene.SceneId]()
                                                        {
                                                            EnterShotMarkingMode(SceneId);
                                                        }
                                                    )
                                                )
                                            );

                                            MenuBuilder.AddMenuSeparator();

                                            MenuBuilder.AddMenuEntry(
                                                FText::FromString(TEXT("Edit Blocking")),
                                                FText::FromString(TEXT("Open existing blocking level")),
                                                FSlateIcon(),
                                                FUIAction(
                                                    FExecuteAction::CreateLambda(
                                                        [this, SceneId = Scene.SceneId]()
                                                        {
                                                            this->OnStartBlockingScene(SceneId);
                                                        }
                                                    )
                                                )
                                            );

                                            MenuBuilder.AddMenuEntry(
                                                FText::FromString("Edit Cast"),
                                                FText::FromString("Modify cast for this blocking scene."),
                                                FSlateIcon(),
                                                FUIAction(
                                                    FExecuteAction::CreateLambda(
                                                        [this, SceneId = Scene.SceneId]()
                                                        {
                                                            this->OpenCastWindowForScene(SceneId);
                                                        }
                                                    )
                                                )
                                            );

                                            MenuBuilder.AddMenuEntry(
                                                FText::FromString(TEXT("Delete Blocking")),
                                                FText::FromString(TEXT("Delete blocking level for this scene")),
                                                FSlateIcon(),
                                                FUIAction(
                                                    FExecuteAction::CreateLambda(
                                                        [this, SceneId = Scene.SceneId]()
                                                        {
                                                            this->OnDeleteBlockingScene(SceneId);
                                                        }
                                                    )
                                                )
                                            );
                                        }


                                        FSlateApplication::Get().PushMenu(
                                            AsShared(),
                                            FWidgetPath(),
                                            MenuBuilder.MakeWidget(),
                                            MouseEvent.GetScreenSpacePosition(),
                                            FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
                                        );

                                        return FReply::Handled();
                                    }

                                    // Left click → scroll to scene
                                    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
                                    {
                                        if (ScriptPanel.IsValid())
                                        {
                                            ScriptPanel->ScrollToBlockId(Scene.SceneId);
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

                    // Length
                    + SSplitter::Slot().Value(ColLength)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Scene.FormattedLength))
                    ]

                    // Time
                    + SSplitter::Slot().Value(ColTime)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Scene.TimeOfDayOverride.ToUpper()))
                    ]


                    // Set
                        +SSplitter::Slot().Value(ColSet)
                        [
                            SNew(SBorder)
                                .Padding(2.f)
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this, SceneId = Scene.SceneId]()
                                            {
                                                for (const FGASBlock& Block : CurrentScript.Blocks)
                                                {
                                                    if (Block.Id == SceneId)
                                                    {
                                                        if (!Block.BlockingLevelPath.IsEmpty())
                                                        {
                                                            return FText::FromString(TEXT("✔"));
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
                                                    {
                                                        return Block.BlockingLevelPath.IsEmpty()
                                                            ? FText::GetEmpty()
                                                            : FText::FromString(Block.BlockingLevelPath);
                                                    }
                                                }

                                                return FText::GetEmpty();
                                            })
                                ]
                        ]


                    // Notes (empty for now)
                    + SSplitter::Slot().Value(ColNotes)
                    [
                        SNew(STextBlock)
                            .Text(FText::GetEmpty())
                    ]

            ];

        // --------------------------------------------------
        // SHOTS (only if scene is expanded)
        // --------------------------------------------------
        if (ExpandedScenes.Contains(Scene.SceneId))
        {
            // -----------------------------
            // Shot Header Row (UI only)
            // -----------------------------
            ShotListContainer->AddSlot()
                .AutoHeight()
                .Padding(28.f, 4.f, 4.f, 2.f)
                [
                    SNew(SSplitter)
                        .Orientation(Orient_Horizontal)

                        // SHOT
                        + SSplitter::Slot().Value(ColScene)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("SHOT")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // TYPE (tight)
                        + SSplitter::Slot().Value(ColType)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("TYPE")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // 1/8
                        + SSplitter::Slot().Value(ColLength)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("1/8")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // DESCRIPTION (long, user-added)
                        + SSplitter::Slot().Value(ColDes)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("DESCRIPTION")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // LENS
                        + SSplitter::Slot().Value(ColLens)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("LENS")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // CAMERA (rig / movement flags)
                        + SSplitter::Slot().Value(ColCamera)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("CAMERA")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]

                        // NOTES (long)
                        + SSplitter::Slot().Value(ColNotes)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("NOTES")))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                        ]
                ];


            // -----------------------------
            // Shot Rows
            // -----------------------------
            for (const FGASShotListShotRow& Shot : Scene.Shots)
            {
                ShotListContainer->AddSlot()
                    .AutoHeight()
                    .Padding(28.f, 2.f, 4.f, 2.f)
                    [
                        SNew(SSplitter)
                            .Orientation(Orient_Horizontal)

                            + SSplitter::Slot().Value(ColScene)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(Shot.ShotNumber))
                            ]
                            + SSplitter::Slot().Value(ColType)
                            [
                                SNew(STextBlock)
                                    .Text(
                                        Shot.ShotType.IsEmpty()
                                        ? FText::FromString(TEXT("—"))
                                        : FText::FromString(Shot.ShotType)
                                    )
                            ]
                            + SSplitter::Slot().Value(ColLength)
                            [
                                SNew(STextBlock)
                                    .Text(
                                        Shot.LengthEighths > 0
                                        ? FText::FromString(GAS_FormatPagesEighths(Shot.LengthEighths))
                                        : FText::FromString(TEXT("—"))
                                    )
                            ]
                            + SSplitter::Slot().Value(ColDes)
                                [
                                    SNew(SEditableTextBox)
                                        .Text(FText::FromString(Shot.Description))
                                        .OnTextCommitted_Lambda(
                                            [this, Shot](const FText& NewText, ETextCommit::Type)
                                            {
                                                this->UpdateShotDescription(
                                                    Shot.ShotId,
                                                    NewText.ToString()
                                                );

                                            }
                                        )
                                ]

                            + SSplitter::Slot().Value(ColLens)
                            [
                                SNew(STextBlock)
                                    .Text(
                                        Shot.Lens.IsEmpty()
                                        ? FText::FromString(TEXT("—"))
                                        : FText::FromString(Shot.Lens)
                                    )
                            ]

                            + SSplitter::Slot().Value(ColCamera)
                            [
                                SNew(STextBlock).Text(FText::FromString(Shot.Camera))
                            ]


                            + SSplitter::Slot().Value(ColNotes)
                            [

                                SNew(SMultiLineEditableTextBox)
                                    .Text(FText::FromString(Shot.Notes))
                                    .AutoWrapText(true)
                                    .OnTextCommitted_Lambda(
                                        [this, Shot](const FText& NewText, ETextCommit::Type)
                                        {
                                            this->UpdateShotNotes(
                                                Shot.ShotId,
                                                NewText.ToString()
                                            );
                                        }
                                    )
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
    return FVector2D(InWidth, InWidth / PreviewAspect);
}


void SGAS_ScriptTab::HandleMapOpened(const FString& Filename, bool bAsTemplate)
{
    UE_LOG(LogTemp, Error, TEXT("🔥 HandleMapOpened FIRED | Filename=%s"), *Filename);

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
    // Blocking flow auto-open cast window (one shot only)
    // ------------------------------------------------------------
    if (bBlockingActive && bPendingAutoOpenCastWindow)
    {
        bPendingAutoOpenCastWindow = false;

        const FString SceneId = ActiveBlockingSceneId.ToString();

        ClosePendingActionWindow();

        if (!SceneId.IsEmpty())
        {
            OpenCastWindowForScene(SceneId);
        }

        return;
    }

    // ------------------------------------------------------------
    // Pending shot mode resume (unchanged)
    // ------------------------------------------------------------
    if (PendingShotModeLevelPath.IsEmpty())
    {
        ClosePendingActionWindow();
        return;
    }

    if (!Filename.Contains(PendingShotModeLevelPath))
    {
        ClosePendingActionWindow();
        return;
    }

    UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMode] Map loaded, resuming"));

    const FString SceneId = PendingShotModeSceneId;

    PendingShotModeSceneId.Empty();
    PendingShotModeLevelPath.Empty();

    if (SceneId.IsEmpty())
    {
        return;
    }

    bIsResumingShotMode = true;
    EnterShotMarkingMode(SceneId);
    bIsResumingShotMode = false;

    PendingShotIntentMarkerId.Empty();
    PendingShotIntentSceneId.Empty();

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

void SGAS_ScriptTab::EnterShotMarkingMode(const FString& SceneId)
{
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

    if (GExitPurge || GIsRequestingExit || IsEngineExitRequested())
    {
        PreviewSceneCapture = nullptr;
        PreviewCaptureComponent = nullptr;
        PreviewSourceCamera = nullptr;
        PreviewCameraActor = nullptr;
        PreviewCameraComponent = nullptr;
        CameraPreviewRenderTarget = nullptr;
        PreviewBrush.Reset();
        return;
    }

    ReleaseCameraPreview();
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

    ActiveBlockingShotId = ShotId;

    UE_LOG(LogGASPrePro, Warning,
        TEXT("SetActiveBlockingShot: %s"),
        *ActiveBlockingShotId.ToString());

    // --------------------------------------------------
    // FIND AND ASSIGN PREVIEW CAMERA
    // --------------------------------------------------
#if WITH_EDITOR
    if (GEditor)
    {
        UWorld* World = GEditor->GetEditorWorldContext().World();
        if (World)
        {
            for (TActorIterator<AGAS_ShotCameraActor> It(World); It; ++It)
            {
                AGAS_ShotCameraActor* Cam = *It;
                if (!IsValid(Cam)) continue;

                if (Cam->ShotGuid == ShotId)
                {
                    PreviewCameraComponent = Cam->CineCamera;

                    UE_LOG(LogTemp, Warning,
                        TEXT("Preview Camera Assigned → %s"),
                        *Cam->GetName());

                    break;
                }
            }
        }
    }
#endif

    if (!Script.IsValid())
    {
        return;
    }

    FGASShotIntent* Shot = Script->ShotIntents.Find(ShotId.ToString());
    if (!Shot)
    {
        return;
    }

    if (Shot->CameraActor.IsValid())
    {
        ACineCameraActor* Cam = Shot->CameraActor.Get();

        if (Cam)
        {
            if (!Shot->CameraLocation.IsNearlyZero() || !Shot->CameraRotation.IsNearlyZero())
            {
                Cam->SetActorLocationAndRotation(
                    Shot->CameraLocation,
                    Shot->CameraRotation
                );

                if (UCineCameraComponent* Cine = Cam->GetCineCameraComponent())
                {
                    Cine->SetCurrentFocalLength(Shot->CameraFocalLength);
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
    BoundShotCameras.Empty();

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return;
    }

    for (TActorIterator<AGAS_ShotCameraActor> It(World); It; ++It)
    {
        AGAS_ShotCameraActor* Cam = *It;
        if (!IsValid(Cam))
        {
            continue;
        }

        // Prevent double-binding
        if (BoundShotCameras.Contains(Cam))
        {
            continue;
        }

        Cam->OnShotCameraMoved.AddSP(this, &SGAS_ScriptTab::HandleShotCameraMoved);
        BoundShotCameras.Add(Cam);
    }
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

    UE_LOG(LogTemp, Warning, TEXT("CAPTURED FINAL PREVIEW CAMERA: %s"), *Location.ToString());
}

void SGAS_ScriptTab::RehydrateShotCamerasForScene(const FString& SceneBlockId)
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
            FoundCam->SetActorTransform(Marker.CameraTransform);
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
            UE_LOG(LogGASPrePro, Warning, TEXT("RehydrateShotCamerasForScene: Failed to spawn camera for marker %s"), *MarkerId);
            continue;
        }

        NewCam->MarkerId = MarkerId;
        ExistingByMarkerId.Add(MarkerId, NewCam);
    }
}


#endif
