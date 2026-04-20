#include "SGAS_ScriptTab.h"
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

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
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
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/FileManager.h"
#include "Factories/WorldFactory.h"
#include "EditorSubsystem.h"
#include "Engine/LevelStreaming.h"



// =======================================================
// Fixed panel widths (UI constants)
// =======================================================
static constexpr float ScriptPanelWidth = 810.f;
static constexpr float ShotListPanelWidth = 910.f;

// =======================================================
// FOR BLOCKING SETUP
// =======================================================
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
                            Options.ShotNumberingPolicy = EGASShotNumberingPolicy::Numeric_10s;
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


void SGAS_ScriptTab::Construct(const FArguments& InArgs)
{

    UE_LOG(LogGASPrePro, Verbose, TEXT("=== GAS SCRIPT TAB CONSTRUCTED ==="));

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
                        .WidthOverride(60.f)
                        .MinDesiredWidth(60.f)
                        .MaxDesiredWidth(60.f)
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
                                .Padding(0, 8, 0, 0)
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
        RebuildCastUI();

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

        bShotSelectArmed = false;

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
        FGASImportNumberingOptions ImportOptions =
            PromptImportNumberingOptions();

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
    SaveScriptToJson();


#if WITH_EDITOR
    TArray<UPackage*> PackagesToSave;

    for (TObjectIterator<UWorld> It; It; ++It)
    {
        UWorld* World = *It;

        if (!World || !World->PersistentLevel)
        {
            continue;
        }

        UPackage* Package = World->GetOutermost();
        if (Package && Package->IsDirty())
        {
            PackagesToSave.Add(Package);
        }
    }

    if (PackagesToSave.Num() > 0)
    {
        FEditorFileUtils::PromptForCheckoutAndSave(
            PackagesToSave,
            /*bCheckDirty=*/ false,
            /*bPromptToSave=*/ false
        );
    }
#endif

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

    TSharedRef<SWindow> CreateWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Create New GAS Project")))
        .ClientSize(FVector2D(400.f, 140.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    CreateWindow->SetContent(
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .Padding(10.f, 10.f, 10.f, 5.f)
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

                                if (CreateNewProject(EnteredName))
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

FReply SGAS_ScriptTab::OnToggleShotMarking()
{
    // Simple toggle for now – we'll wire this into real behavior later.
    bIsShotMarkingActive = !bIsShotMarkingActive;

    UE_LOG(LogGASPrePro, Log, TEXT("Shot marking %s"),
        bIsShotMarkingActive ? TEXT("ENABLED") : TEXT("DISABLED"));

    return FReply::Handled();
}

FReply SGAS_ScriptTab::OnAddShotMarkerClicked()
{
    bShotSelectArmed = !bShotSelectArmed;

    UE_LOG(LogGASPrePro, Warning, TEXT("[ShotMarker] Shot mode = %s"),
        bShotSelectArmed ? TEXT("ON") : TEXT("OFF"));

    if (ScriptPanel.IsValid())
    {
        ScriptPanel->SetShotAddArmed(bShotSelectArmed);
    }

    return FReply::Handled();
}

bool SGAS_ScriptTab::CreateNewProject(const FString& ProjectName)
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

    if (!NewProject)
    {
        return false;
    }

    // Assign name
    NewProject->ProjectName = CleanName;

    // Mark dirty and register
    NewProject->MarkPackageDirty();
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

    UE_LOG(LogGASPrePro, Verbose, TEXT("PROJECT LOADED: %s"), *ActiveProject->ProjectName);

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
    FGASScript Script;

    // Import options now come directly from the import dialog
    const FGASImportNumberingOptions& Options = ImportOptions;



    if (!UGAS_FDXImporter::ImportFDXToScript(FilePath, Script, Options))
    {
        UE_LOG(LogGASPrePro, Error, TEXT("FDX import failed: %s"), *FilePath);
        return;
    }




    // --------------------------------------------------------------------
    // 4. Populate authoritative in-memory script (JSON-backed)
    // --------------------------------------------------------------------
    CurrentScript = Script;
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
    CurrentScript.ShotNumberingPolicy = ImportOptions.ShotNumberingPolicy;



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

    TArray<FRenderedParagraph> Rendered =
        ScriptLayoutEngine::LayoutScript(
            CurrentScript.Blocks,
            PanelWidth,
            CurrentScript.UserPageBreaks,
            CurrentScript.SceneNumbering
        );

    ScriptPanel->SetRenderedScript(Rendered);

    TArray<FGASShotListSceneRow> SceneRowsV2;

    BuildShotListFromScriptV2(
        CurrentScript,
        CurrentScript.SceneNumbering,
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


// ============================================================================
// BLOCKING FUNCTIONS
// ============================================================================

void SGAS_ScriptTab::OnStartBlockingScene(const FString& SceneId)
{
    if (!ActiveProject)
    {
        return;
    }

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
        FEditorFileUtils::LoadMap(SceneBlock->BlockingLevelPath, false, true);

        bBlockingActive = true;
        ActiveBlockingSceneId = FGuid(SceneId);

        GASBlockingAccess::SetBlockingActive(true);
        GASBlockingAccess::SetActiveSceneId(ActiveBlockingSceneId);

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

    const FString ProjectName = ActiveProject->ProjectName;
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

    // Find zero-based scene index
    int32 ZeroBasedSceneIndex = 0;

    for (int32 i = 0; i < CurrentScript.Blocks.Num(); ++i)
    {
        if (CurrentScript.Blocks[i].Type == EGASBlockType::SceneHeading)
        {
            if (CurrentScript.Blocks[i].Id == SceneBlock.Id)
            {
                break;
            }
            ZeroBasedSceneIndex++;
        }
    }

    FString NumberPart = GASSceneNumbering::MakeSceneNumber(
        ZeroBasedSceneIndex,
        CurrentScript.SceneNumbering.BaseStyle
    );

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

    // Activate blocking
    bBlockingActive = true;
    ActiveBlockingSceneId = FGuid(SceneBlock.Id);

    GASBlockingAccess::SetBlockingActive(true);
    GASBlockingAccess::SetActiveSceneId(ActiveBlockingSceneId);

    // Stream selected set
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogGASPrePro, Error, TEXT("Blocking: Editor world null."));
        return false;
    }


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

void SGAS_ScriptTab::OpenCastWindowForScene(FString SceneId)
{
    // Activate blocking state
    bBlockingActive = true;
    ActiveBlockingSceneId = FGuid(SceneId);

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
        .ClientSize(FVector2D(600.f, 400.f));

    Window->SetContent(
        SNew(SGAS_BlockingCastWindow)
        .SceneId(ActiveBlockingSceneId)
        .Script(&CurrentScript)
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

            // Do NOT stop blocking here anymore.
            UE_LOG(LogGASPrePro, Warning, TEXT("Cast window closed (blocking still active)"));
        }
    ));

    FSlateApplication::Get().AddWindow(Window);
}

void SGAS_ScriptTab::OnBlockingCastModified()
{
    UE_LOG(LogGASPrePro, Warning, TEXT("Blocking cast modified — saving"));

    MarkScriptDirty();
    SaveScriptToJson();

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (World)
    {
        World->MarkPackageDirty();
    }
}

void SGAS_ScriptTab::SpawnSceneCast(FGASBlock* SceneBlock)
{
    if (!SceneBlock)
    {
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return;
    }

    // Spawn into persistent level explicitly
    ULevel* TargetLevel = World->PersistentLevel;
    if (!TargetLevel)
    {
        return;
    }

    float XOffset = 0.f;

    for (const FString& CharacterName : SceneBlock->CharactersInScene)
    {

        // -----------------------------------------------------
        // Prevent duplicate cast spawn
        // -----------------------------------------------------
        FString UpperName = CharacterName.ToUpper();

        bool bAlreadyExists = false;

        for (AActor* Existing : TargetLevel->Actors)
        {
            if (!Existing)
                continue;

            if (Existing->GetActorLabel() == UpperName)
            {
                bAlreadyExists = true;
                break;
            }
        }

        if (bAlreadyExists)
        {
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

            // Make selectable + undoable
            StandIn->SetFlags(RF_Transactional);
            StandIn->SetIsTemporarilyHiddenInEditor(false);
            StandIn->SetActorEnableCollision(true);
            StandIn->SetActorHiddenInGame(false);
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
    UEditorLoadingAndSavingUtils::LoadMap(SceneBlock->BlockingLevelPath);

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

    // -----------------------------------------------------
    // Spawn cast BEFORE saving
    // -----------------------------------------------------
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

    // Close window
    if (ActiveSetBrowserWindow.IsValid())
    {
        ActiveSetBrowserWindow->RequestDestroyWindow();
        ActiveSetBrowserWindow.Reset();
    }

    PendingBlockingSceneId.Empty();
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
    // SHOT MARKING MODE (Phase 1 enforced)
    // -------------------------------------------------
    if (bIsShotMarkingActive)
    {
        const FString& BlockId = CurrentScript.Blocks[BlockIndex].Id;

        FGASMarker NewMarker;
        NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        NewMarker.MarkerType = EGASMarkerType::ShotMarker;
        NewMarker.ShotOrigin = EGASShotOrigin::User;
        NewMarker.BlockId = BlockId;

        NewMarker.Metadata.Add(TEXT("Type"), TEXT("—"));
        NewMarker.Metadata.Add(TEXT("Lens"), TEXT("—"));
        NewMarker.Metadata.Add(TEXT("Camera"), TEXT("Handheld"));
        NewMarker.Metadata.Add(TEXT("Description"), TEXT(""));

        CurrentScript.Markers.Add(NewMarker);

        RebuildShotList();
        MarkScriptDirty();

        return;
    }



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
    bShotSelectArmed = false;

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
    if (!ActiveProject || ActiveProject->ProjectName.IsEmpty())
    {
        return TEXT("");
    }

    FString BaseDir =
        FPaths::ProjectSavedDir() + TEXT("GAS/");

    FString ProjectDir =
        BaseDir + ActiveProject->ProjectName + TEXT("/");

    IFileManager::Get().MakeDirectory(*ProjectDir, true);

    return ProjectDir + TEXT("Script.gasjson");
}

void SGAS_ScriptTab::ScrollToSceneBlock(const FString& BlockId)
{
    if (!ScriptPanel.IsValid())
        return;

    ScriptPanel->ScrollToBlockId(BlockId);
}

void SGAS_ScriptTab::AddShotMarkerForScene(const FString& SceneBlockId)
{
    FGASMarker NewMarker;

    if (!FGASShotMarkerFactory::CreateShotMarkerForSceneHeading(
        CurrentScript.Blocks,
        SceneBlockId,
        NewMarker))
    {
        return;
    }

    CurrentScript.Markers.Add(NewMarker);


    RebuildShotList();
}

FSlateColor SGAS_ScriptTab::GetShotButtonTint() const
{
    return bShotSelectArmed
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

void SGAS_ScriptTab::UpdateCameraFromShotIntent(const FGASShotIntent& Intent)
{
    // TEMP STUB – restore compile
}

FGASScript& SGAS_ScriptTab::GetScriptMutable()
{
    return CurrentScript;
}

void SGAS_ScriptTab::ClearShotSelectionAfterDelete()
{
    ActiveShotMarkerId.Empty();
}

void SGAS_ScriptTab::OnShotIntentChanged()
{
    OnShotListNeedsRefresh.Broadcast();
}