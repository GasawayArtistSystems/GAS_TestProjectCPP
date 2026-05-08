#include "UI/Blocking/SGAS_BlockingCastWindow.h"
#include "GAS_StandInActor.h"
#include "GAS_SequencerBindingUtils.h"

#include "ScriptModel.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/SkeletalMesh.h"
#include "Modules/ModuleManager.h"

#include "Containers/Ticker.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"


static ULevelSequence* GetActiveBlockingSequence()
{
    if (ULevelSequence* CurrentSequence =
        ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence())
    {
        return CurrentSequence;
    }

    return nullptr;
}

void SGAS_BlockingCastWindow::Construct(const FArguments& InArgs)
{
    SceneId = InArgs._SceneId;
    Script = InArgs._Script;
    OnCastModified = InArgs._OnCastModified;

    UE_LOG(LogTemp, Warning, TEXT("BlockingCastWindow Construct START"));

    if (!Script)
    {
        UE_LOG(LogTemp, Error, TEXT("BlockingCastWindow: Script NULL"));

        ChildSlot
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("SCRIPT NULL")))
            ];

        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("BlockingCastWindow: Script OK"));

    BlockingSequence = GetActiveBlockingSequence();

    if (BlockingSequence.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("BlockingCastWindow: Active sequence = %s"),
            *BlockingSequence->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("BlockingCastWindow: No active sequence on construct"));
    }


    // ---------------------------------------------------------
    // Build Left List (Script.Cast)
    // ---------------------------------------------------------
    LeftItems.Reset();

    for (const FGASCharacterDefinition& Def : Script->Cast)
    {
        LeftItems.Add(MakeShared<FString>(Def.CharacterName.ToUpper()));
    }

    // ---------------------------------------------------------
    // Build Right List (SceneBlock.CharactersInScene)
    // ---------------------------------------------------------
    RightItems.Reset();

    for (const FGASBlock& Block : Script->Blocks)
    {
        if (Block.Id == SceneId.ToString())
        {
            for (const FString& Name : Block.CharactersInScene)
            {
                RightItems.Add(MakeShared<FString>(Name.ToUpper()));
            }
            break;
        }
    }

    // ---------------------------------------------------------
    // Character Mesh Asset List
    // ---------------------------------------------------------
    AvailableCharacterMeshes.Reset();

    FAssetRegistryModule& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    TArray<FAssetData> MeshAssets;

    AssetRegistry.Get().GetAssetsByPath(
        FName("/Game/Characters"),
        MeshAssets,
        true
    );

    for (const FAssetData& Asset : MeshAssets)
    {
        if (Asset.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
        {
            AvailableCharacterMeshes.Add(MakeShared<FAssetData>(Asset));
        }
    }

    AvailableCharacterMeshes.Sort(
        [](const TSharedPtr<FAssetData>& A, const TSharedPtr<FAssetData>& B)
        {
            return A->AssetName.LexicalLess(B->AssetName);
        }
    );

    // ---------------------------------------------------------
    // UI
    // ---------------------------------------------------------
    ChildSlot
        [
            SNew(SVerticalBox)

                // -------------------------------------------------
                // SCENE HEADER
                // -------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8, 8, 8, 4)
                [
                    SNew(STextBlock)
                        .Text_Lambda([this]()
                            {
                                if (!Script)
                                    return FText::FromString(TEXT("UNKNOWN SCENE"));

                                const FString SceneIdStr = SceneId.ToString();

                                for (const FGASBlock& Block : Script->Blocks)
                                {
                                    if (Block.Id == SceneIdStr)
                                    {
                                        if (!Block.Text.IsEmpty())
                                        {
                                            return FText::FromString(Block.Text.ToUpper());
                                        }
                                    }
                                }

                                return FText::FromString(TEXT("UNKNOWN SCENE"));
                            })
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                ]

            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8, 0, 8, 8)
                [
                    SNew(SSeparator)
                ]

                // -------------------------------------------------
                // MAIN CONTENT
                // -------------------------------------------------
                +SVerticalBox::Slot()
                .FillHeight(1.f)
                [
                    SNew(SHorizontalBox)

                        // LEFT COLUMN
                        + SHorizontalBox::Slot()
                        .FillWidth(1.f)
                        [
                            SNew(SVerticalBox)

                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(6)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("CAST")))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                                ]

                                + SVerticalBox::Slot()
                                .FillHeight(1.f)
                                [
                                    SAssignNew(LeftListView, SListView<TSharedPtr<FString>>)
                                        .ListItemsSource(&LeftItems)
                                        .OnSelectionChanged_Lambda(
                                            [this](TSharedPtr<FString> Item, ESelectInfo::Type)
                                            {
                                                SelectedLeftItem = Item;
                                            }
                                        )
                                        .OnGenerateRow_Lambda(
                                            [this](TSharedPtr<FString> Item,
                                                const TSharedRef<STableViewBase>& Owner)
                                            {
                                                FString CurrentMeshName = TEXT("None");

                                                for (const FGASCharacterDefinition& Def : Script->Cast)
                                                {
                                                    if (Def.CharacterName.ToUpper() == *Item)
                                                    {
                                                        if (!Def.DefaultMeshPath.IsEmpty())
                                                        {
                                                            CurrentMeshName = FPackageName::GetShortName(Def.DefaultMeshPath);
                                                        }
                                                        break;
                                                    }
                                                }

                                                return SNew(STableRow<TSharedPtr<FString>>, Owner)
                                                    [
                                                        SNew(SHorizontalBox)

                                                            // Character Name
                                                            + SHorizontalBox::Slot()
                                                            .FillWidth(0.5f)
                                                            [
                                                                SNew(STextBlock)
                                                                    .Text(FText::FromString(*Item))
                                                            ]

                                                            // Default Mesh Dropdown
                                                            + SHorizontalBox::Slot()
                                                            .FillWidth(0.5f)
                                                            [
                                                                SNew(SComboBox<TSharedPtr<FAssetData>>)
                                                                    .OptionsSource(&AvailableCharacterMeshes)

                                                                    .OnGenerateWidget_Lambda([](TSharedPtr<FAssetData> Asset)
                                                                        {
                                                                            return SNew(STextBlock)
                                                                                .Text(FText::FromName(Asset->AssetName));
                                                                        })

                                                                    .OnSelectionChanged_Lambda([this, Item](TSharedPtr<FAssetData> Asset, ESelectInfo::Type)
                                                                        {
                                                                            if (!Asset.IsValid())
                                                                                return;

                                                                            // -------------------------------------------------
                                                                            // Update Script Default Mesh
                                                                            // -------------------------------------------------
                                                                            const FGASCharacterDefinition* Def =
                                                                                Script->FindCharacterDefinition(*Item);

                                                                            if (Def)
                                                                            {
                                                                                FGASCharacterDefinition* MutableDef =
                                                                                    const_cast<FGASCharacterDefinition*>(Def);

                                                                                MutableDef->DefaultMeshPath = Asset->GetObjectPathString();

                                                                                // -------------------------------------------------
                                                                                // Refresh all stand-ins in the level
                                                                                // -------------------------------------------------

                                                                                UWorld* World = GEditor->GetEditorWorldContext().World();

                                                                                if (World)
                                                                                {
                                                                                    for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
                                                                                    {
                                                                                        AGAS_StandInActor* StandIn = *It;

                                                                                        if (!StandIn)
                                                                                            continue;

                                                                                        if (StandIn->GAS_CharacterId == *Item)
                                                                                        {
                                                                                            StandIn->RefreshMeshFromScript(Script);
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }

                                                                            // -------------------------------------------------
                                                                            // Update Existing Stand-Ins in Scene
                                                                            // -------------------------------------------------
                                                                            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

                                                                            if (World)
                                                                            {
                                                                                for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
                                                                                {
                                                                                    if (It->GAS_CharacterId == *Item)
                                                                                    {
                                                                                        It->RefreshMeshFromScript(Script);
                                                                                    }
                                                                                }
                                                                            }

                                                                            // Refresh scene list
                                                                            RightListView->RequestListRefresh();
                                                                        })

                                                                    [
                                                                        SNew(STextBlock)
                                                                            .Text(FText::FromString(CurrentMeshName))
                                                                    ]
                                                            ]
                                                    ];
                                            })
                                ]
                        ]

                    // CENTER BUTTONS
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(16, 0)
                        [
                            SNew(SVerticalBox)

                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 10)
                                [
                                    SNew(SButton)
                                        .Text(FText::FromString(TEXT("ADD →")))
                                        .OnClicked(this, &SGAS_BlockingCastWindow::OnAddClicked)
                                ]

                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0, 10)
                                [
                                    SNew(SButton)
                                        .Text(FText::FromString(TEXT("← REMOVE")))
                                        .OnClicked(this, &SGAS_BlockingCastWindow::OnRemoveClicked)
                                ]
                        ]

                    // RIGHT COLUMN
                    + SHorizontalBox::Slot()
                        .FillWidth(1.f)
                        [
                            SNew(SVerticalBox)

                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(6)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("IN SCENE")))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                                ]

                                + SVerticalBox::Slot()
                                .FillHeight(1.f)
                                [
                                    SAssignNew(RightListView, SListView<TSharedPtr<FString>>)
                                        .ListItemsSource(&RightItems)
                                        .OnSelectionChanged_Lambda(
                                            [this](TSharedPtr<FString> Item, ESelectInfo::Type)
                                            {
                                                SelectedRightItem = Item;
                                            }
                                        )
                                        .OnGenerateRow_Lambda(
                                            [this](TSharedPtr<FString> Item,
                                                const TSharedRef<STableViewBase>& Owner)
                                            {
                                                return SNew(STableRow<TSharedPtr<FString>>, Owner)
                                                    [
                                                        SNew(SHorizontalBox)

                                                            // Character Name
                                                            + SHorizontalBox::Slot()
                                                            .FillWidth(0.5f)
                                                            .VAlign(VAlign_Center)
                                                            [
                                                                SNew(STextBlock)
                                                                    .Text(FText::FromString(*Item))
                                                            ]

                                                            // Character Mesh Dropdown
                                                            + SHorizontalBox::Slot()
                                                            .FillWidth(0.5f)
                                                            .VAlign(VAlign_Center)
                                                            [
                                                                SNew(SComboBox<TSharedPtr<FAssetData>>)
                                                                    .OptionsSource(&AvailableCharacterMeshes)


                                                                    .OnGenerateWidget_Lambda([](TSharedPtr<FAssetData> Asset)
                                                                        {
                                                                            return SNew(STextBlock)
                                                                                .Text(FText::FromName(Asset->AssetName));
                                                                        })

                                                                    .OnSelectionChanged_Lambda([Item](TSharedPtr<FAssetData> Asset, ESelectInfo::Type)
                                                                        {
                                                                            if (!Asset.IsValid())
                                                                                return;

                                                                            USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset->GetAsset());
                                                                            if (!Mesh)
                                                                                return;

                                                                            UWorld* World = GEditor->GetEditorWorldContext().World();

                                                                            for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
                                                                            {
                                                                                if (It->GAS_CharacterId == *Item)
                                                                                {
                                                                                    It->SetCharacterSkeletalMesh(Mesh);
                                                                                    break;
                                                                                }
                                                                            }
                                                                        })

                                                                    [
                                                                        SNew(STextBlock)
                                                                            .Text_Lambda([this, Item]()
                                                                                {
                                                                                    UWorld* World = GEditor->GetEditorWorldContext().World();

                                                                                    // -------------------------------------------------
                                                                                    // First check if a stand-in actor has an override mesh
                                                                                    // -------------------------------------------------
                                                                                    for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
                                                                                    {
                                                                                        if (It->GAS_CharacterId == *Item)
                                                                                        {
                                                                                            USkeletalMesh* Mesh = It->GetCharacterMesh();

                                                                                            if (Mesh)
                                                                                            {
                                                                                                return FText::FromString(Mesh->GetName());
                                                                                            }
                                                                                        }
                                                                                    }

                                                                                    // -------------------------------------------------
                                                                                    // Otherwise fall back to script default mesh
                                                                                    // -------------------------------------------------
                                                                                    FGASCharacterDefinition* Def =
                                                                                        const_cast<FGASCharacterDefinition*>(
                                                                                            Script->FindCharacterDefinition(*Item)
                                                                                            );

                                                                                    if (Def && !Def->DefaultMeshPath.IsEmpty())
                                                                                    {
                                                                                        return FText::FromString(
                                                                                            FPackageName::GetShortName(Def->DefaultMeshPath)
                                                                                        );
                                                                                    }

                                                                                    return FText::FromString(TEXT("None"));
                                                                                })
                                                                    ]
                                                            ]
                                                    ];
                                            })
                                ]
                        ]
                ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    .Padding(8, 8)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("Close")))
                            .OnClicked_Lambda([this]()
                                {
                                    if (TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
                                    {
                                        Window->RequestDestroyWindow();
                                    }
                                    return FReply::Handled();
                                })
                    ]
        ];

}

void SGAS_BlockingCastWindow::RefreshLists()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    if (World && Script)
    {
        for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
        {
            AGAS_StandInActor* Actor = *It;

            if (!Actor)
                continue;

            const FString ActorName = Actor->GAS_CharacterId.ToUpper();

            for (const FGASCharacterDefinition& Def : Script->Cast)
            {
                if (Def.CharacterName.ToUpper() == ActorName)
                {
                    if (!Def.DefaultMeshPath.IsEmpty())
                    {
                        USkeletalMesh* Mesh =
                            LoadObject<USkeletalMesh>(nullptr, *Def.DefaultMeshPath);

                        if (Mesh)
                        {
                            Actor->SetCharacterSkeletalMesh(Mesh);
                        }
                    }

                    break;
                }
            }
        }
    }

    LeftListView->RequestListRefresh();
    RightListView->RequestListRefresh();
}

FReply SGAS_BlockingCastWindow::OnAddClicked()
{
    if (!Script || !SelectedLeftItem.IsValid())
        return FReply::Handled();

    FGASBlock* SceneBlock = nullptr;

    for (FGASBlock& Block : Script->Blocks)
    {
        if (Block.Id == SceneId.ToString())
        {
            SceneBlock = &Block;
            break;
        }
    }

    if (!SceneBlock)
        return FReply::Handled();

    const FString Name = SelectedLeftItem->ToUpper();

    // Prevent duplicates at DATA level
    if (SceneBlock->CharactersInScene.Contains(Name))
        return FReply::Handled();

    SceneBlock->CharactersInScene.Add(Name);


    // -----------------------------------------------------
    // Spawn StandIn Actor (if not already present)
    // -----------------------------------------------------
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    if (World)
    {
        bool bAlreadySpawned = false;

        for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
        {
            if (It->GAS_CharacterId == Name)
            {
                bAlreadySpawned = true;
                break;
            }
        }

        if (!bAlreadySpawned)
        {
            const int32 Index = SceneBlock->CharactersInScene.Num();

            FVector SpawnLocation = FVector(0.f, Index * 120.f, 0.f);
            FActorSpawnParameters Params;

            Params.OverrideLevel = World->PersistentLevel;
            Params.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            AGAS_StandInActor* NewActor =
                World->SpawnActor<AGAS_StandInActor>(
                    AGAS_StandInActor::StaticClass(),
                    SpawnLocation,
                    FRotator::ZeroRotator,
                    Params
                );

            if (NewActor)
            {
                if (!IsValid(NewActor))
                {
                    UE_LOG(LogTemp, Error, TEXT("Spawned actor invalid"));
                    return FReply::Handled();
                }

                NewActor->SetFlags(RF_Transactional);
                NewActor->GAS_CharacterId = Name;
                NewActor->SetActorLabel(Name);

                // -----------------------------------------------------
                // Apply Script Default Mesh
                // -----------------------------------------------------
                for (const FGASCharacterDefinition& Def : Script->Cast)
                {
                    if (Def.CharacterName.ToUpper() == Name)
                    {
                        if (!Def.DefaultMeshPath.IsEmpty())
                        {
                            USkeletalMesh* DefaultMesh =
                                LoadObject<USkeletalMesh>(nullptr, *Def.DefaultMeshPath);

                            if (DefaultMesh)
                            {
                                NewActor->SetCharacterSkeletalMesh(DefaultMesh);
                            }
                        }
                        break;
                    }
                }

                ULevelSequence* Sequence = BlockingSequence.Get();

                if (IsValid(Sequence))
                {
                    FGASSequencerBindingUtils::EnsureActorTransformTrack(
                        Sequence,
                        NewActor
                    );
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("BlockingSequence INVALID"));
                }
            }
        }
    }


    // Prevent duplicates at UI level
    bool bAlreadyInRight = false;
    for (const TSharedPtr<FString>& Item : RightItems)
    {
        if (Item.IsValid() && *Item == Name)
        {
            bAlreadyInRight = true;
            break;
        }
    }

    LeftItems.Remove(SelectedLeftItem);

    if (!bAlreadyInRight)
    {
        RightItems.Add(MakeShared<FString>(Name));
    }

    SelectedLeftItem.Reset();

    if (World)
    {
        World->MarkPackageDirty();
    }

    RefreshLists();

    OnCastModified.ExecuteIfBound();

    return FReply::Handled();
}


FReply SGAS_BlockingCastWindow::OnRemoveClicked()
{
    if (!Script || !SelectedRightItem.IsValid())
        return FReply::Handled();

    FGASBlock* SceneBlock = nullptr;

    for (FGASBlock& Block : Script->Blocks)
    {
        if (Block.Id == SceneId.ToString())
        {
            SceneBlock = &Block;
            break;
        }
    }

    if (!SceneBlock)
        return FReply::Handled();

    const FString Name = SelectedRightItem->ToUpper();

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    AGAS_StandInActor* ActorToRemove = nullptr;

    if (World)
    {
        for (TActorIterator<AGAS_StandInActor> It(World); It; ++It)
        {
            if (It->GAS_CharacterId == Name)
            {
                ActorToRemove = *It;
                break;
            }
        }

        ULevelSequence* Sequence = nullptr;

        if (BlockingSequence.IsValid())
        {
            Sequence = BlockingSequence.Get();
        }
        else
        {
            Sequence = GetActiveBlockingSequence();
            BlockingSequence = Sequence;
        }

        // 🔥 REMOVE FROM SCRIPT FIRST
        SceneBlock->CharactersInScene.Remove(Name);

        if (IsValid(Sequence) && ActorToRemove)
        {
            FGASSequencerBindingUtils::RemoveCharacterFromSequencer(
                Sequence,
                ActorToRemove
            );

            Sequence->Modify();
            Sequence->GetMovieScene()->Modify();
            Sequence->MarkPackageDirty();
        }


        if (ActorToRemove)
        {
            UE_LOG(LogTemp, Error, TEXT("🔥 DESTROYING ACTOR IMMEDIATELY"));

            ActorToRemove->Modify();
            ActorToRemove->Destroy();
            ActorToRemove = nullptr;
        }

        World->MarkPackageDirty();
    }

    RightItems.RemoveAll(
        [&](const TSharedPtr<FString>& Item)
        {
            return Item.IsValid() && *Item == Name;
        }
    );

    bool bAlreadyInLeft = false;

    for (const TSharedPtr<FString>& Item : LeftItems)
    {
        if (Item.IsValid() && *Item == Name)
        {
            bAlreadyInLeft = true;
            break;
        }
    }

    if (!bAlreadyInLeft)
    {
        LeftItems.Add(MakeShared<FString>(Name));
    }

    SelectedRightItem.Reset();

    RefreshLists();

    if (Script)
    {
        // save only, no global cast rebuild
        OnCastModified.ExecuteIfBound();
    }

    return FReply::Handled();
}

