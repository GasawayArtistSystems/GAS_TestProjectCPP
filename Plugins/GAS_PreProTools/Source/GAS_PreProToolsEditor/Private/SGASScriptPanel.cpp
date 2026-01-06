// NOTE:
// This panel NEVER mutates script content.
// All edits go through the script edit pipeline (JSON → ReLayout).


#include "SGASScriptPanel.h"
#include "ScriptFormatRules.h"
#include "SlateOptMacros.h"
#include "GAS_PreProToolsStyle.h"
#include "Modules/ModuleManager.h"
#include "GAS_PreProToolsEditorModule.h"
#include "GAS_SceneNumberResolver.h"
#include "GAS_ImportNumberingTypes.h"
#include "GAS_SceneNumberResolver.h"


#include "ScriptLayoutEngine.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

#include "SlateBasics.h"
#include "SlateExtras.h"
#include "Containers/Ticker.h"

// =============================================================
//  Constants
// =============================================================

static constexpr float ShotPillWidth = 48.f;
static constexpr float ShotPillHeight = 18.f;

// Shot drag (ALT + LMB)
bool bIsDraggingShot = false;
FString DraggingShotMarkerId;
FVector2D ShotDragOffset;
float ShotDragGhostX = -1.f;
float ShotDragGhostY = -1.f;

// Shot tail resize (ALT + LMB on tail)
bool bIsResizingShotTail = false;
FString ResizingShotMarkerId;
float ShotTailGhostY = -1.f;




// =============================================================
//  Construct
// =============================================================
void SGASScriptPanel::Construct(const FArguments& InArgs)
{
    OnParagraphClicked = InArgs._OnParagraphClicked;

    SetCanTick(true);

    bNeedsRedraw = true;

    ChildSlot
        [
            SNew(SBorder)
                .Padding(0)
                .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                .Clipping(EWidgetClipping::ClipToBoundsAlways)
        ];

}

void SGASScriptPanel::RebuildLayout()
{
    if (!SourceScript)
        return;

    // Panel width: fall back to something sane if geometry is tiny at startup
    float PanelWidth = GetCachedGeometry().GetLocalSize().X;
    if (PanelWidth < 200.f)
    {
        PanelWidth = 1100.f;
    }

    TArray<FRenderedParagraph> NewRendered =
        ScriptLayoutEngine::LayoutScript(
            SourceScript->Blocks,
            PanelWidth,
            SourceScript->UserPageBreaks,
            SourceScript->SceneNumbering
        );



    SetRenderedScript(NewRendered);   // updates RenderedParagraphs + bNeedsRedraw

    // ------------------------------------------------------------
    // Auto-adjust shot tail to paragraph end
    // (keeps tail attached when text grows)
    // ------------------------------------------------------------
    if (SourceScript)
    {
        for (FGASMarker& M : SourceScript->Markers)
        {
            if (M.MarkerType != EGASMarkerType::ShotMarker)
                continue;

            if (!RenderedParagraphs.IsValidIndex(M.ParagraphIndex))
                continue;

            // Only adjust tails that are defined
            if (M.ShotLineEndY < 0.f)
                continue;

            const FRenderedParagraph& P =
                RenderedParagraphs[M.ParagraphIndex];

            const float ParagraphEndY =
                P.TopY + P.Height;

            // Clamp tail to paragraph end if it was at or below it
            if (M.ShotLineEndY <= ParagraphEndY + ScriptFormat::LineHeight)
            {
                M.ShotLineEndY = ParagraphEndY;
            }
        }
    }


    if (OnRequestFullRedraw.IsBound())
    {
        OnRequestFullRedraw.Execute();
    }

}

void SGASScriptPanel::SetScript(FGASScript* InScript)
{
    SourceScript = InScript;
    bNeedsRedraw = true;
}


// =============================================================
//  SetRenderedScript
// =============================================================
void SGASScriptPanel::SetRenderedScript(const TArray<FRenderedParagraph>& InParagraphs)
{
    RenderedParagraphs = InParagraphs;
    bNeedsRedraw = true;
}

// =============================================================
//  ComputeDesiredSize
// =============================================================
FVector2D SGASScriptPanel::ComputeDesiredSize(float) const
{
    return FVector2D(1000.f, CachedTotalHeight);
}

// =====================================================
// SCRIPT EDIT PIPELINE
// =====================================================

void SGASScriptPanel::ApplyScriptEdit(const FGASScriptEdit& Edit)
{
    if (!SourceScript)
        return;

    if (Edit.ParagraphIndex == INDEX_NONE)
        return;

    switch (Edit.Type)
    {
    case EGASScriptEditType::SetPageBreak:
    {
        const int32 ParagraphIndex = Edit.ParagraphIndex;

        if (RenderedParagraphs.IsValidIndex(ParagraphIndex))
        {
            const FString& BlockId =
                RenderedParagraphs[ParagraphIndex].BlockId;

            // Prevent duplicate page breaks on the same block
            bool bAlreadyHasBreak = false;
            for (const FGASUserPageBreak& PB : SourceScript->UserPageBreaks)
            {
                if (PB.AfterBlockId == BlockId)
                {
                    bAlreadyHasBreak = true;
                    break;
                }
            }

            if (!bAlreadyHasBreak)
            {
                FGASUserPageBreak NewBreak;
                NewBreak.AfterBlockId = BlockId;

                SourceScript->UserPageBreaks.Add(NewBreak);
            }
        }
        break;
    }





    case EGASScriptEditType::ClearPageBreak:
    {
        const int32 ParagraphIndex = Edit.ParagraphIndex;

        if (RenderedParagraphs.IsValidIndex(ParagraphIndex))
        {
            const FString& BlockId =
                RenderedParagraphs[ParagraphIndex].BlockId;

            SourceScript->UserPageBreaks.RemoveAll(
                [&](const FGASUserPageBreak& PB)
                {
                    return PB.AfterBlockId == BlockId;
                }
            );
        }
        break;
    }




    default:
        break;
    }



    // Always re-layout after edits so visuals match JSON
    RebuildLayout();
    Invalidate(EInvalidateWidget::LayoutAndVolatility);

}

void SGASScriptPanel::ApplyTextEdit(
    const FString& BlockId,
    const FString& NewText)
{
    if (!SourceScript)
        return;

    for (FGASBlock& Block : SourceScript->Blocks)
    {
        if (Block.Id == BlockId)
        {
            Block.Text = NewText;
            break;
        }
    }

    if (OnScriptMutated.IsBound())
    {
        UE_LOG(LogTemp, Warning, TEXT("SCRIPT MUTATED → notifying owner"));
        OnScriptMutated.Execute();
    }


    RebuildLayout();
}


int32 SGASScriptPanel::HitTestAddGutter(float MouseY) const
{
    if (RenderedParagraphs.Num() < 1)
    {
        return INDEX_NONE;
    }

    constexpr float GutterTolerance = 12.f;

    for (int32 i = 0; i < RenderedParagraphs.Num() - 1; ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        const float GutterY =
            P.TopY + P.Height + (ScriptFormat::LineHeight * 0.25f);

        if (FMath::Abs(MouseY - GutterY) <= GutterTolerance)
        {
            return P.ParagraphIndex; // insert AFTER this paragraph
        }
    }

    return INDEX_NONE;
}

void SGASScriptPanel::TryEditParagraph(int32 BlockIndex)
{
    if (!SourceScript)
        return;

    if (!SourceScript->Blocks.IsValidIndex(BlockIndex))
        return;

    // 🔑 SELECT FIRST (so highlight updates immediately)
    SelectedParagraphIndex = BlockIndex;
    Invalidate(EInvalidateWidget::Paint);

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    switch (Block.Type)
    {
    case EGASBlockType::Action:
        OpenEditActionDialog(BlockIndex);
        break;

    case EGASBlockType::Character:
        OpenEditCharacterDialog(BlockIndex);
        break;

    case EGASBlockType::Dialogue:
        OpenEditDialogueDialog(BlockIndex);
        break;

    default:
        break;
    }
}


void SGASScriptPanel::OpenEditActionDialog(int32 BlockIndex)
{

    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("OpenEditActionDialog: SourceScript null or invalid index %d"), BlockIndex);
        return;
    }

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];


    TSharedRef<SWindow> EditWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Edit Action")))
        .ClientSize(FVector2D(600.f, 300.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    TSharedPtr<SMultiLineEditableTextBox> TextBox;

    EditWindow->SetContent(
        SNew(SBorder)
        [
            SNew(SVerticalBox)

                // ----------------------------------------------------
                // Text box
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .FillHeight(1.f)
                .Padding(8.f)
                [
                    SAssignNew(EditTextBox, SMultiLineEditableTextBox)
                        .Text(FText::FromString(Block.Text))
                        .OnTextChanged_Lambda([this](const FText& NewText)
                            {
                                CachedEditText = NewText;
                            })
                ]

            // ----------------------------------------------------
            // Buttons
            // ----------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(8.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([this, EditWindow]()
                                    {
                                        OnEditCancelled();
                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })

                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("OK")))
                                .OnClicked_Lambda([this, EditWindow, BlockIndex]()
                                    {
                                        if (SourceScript && SourceScript->Blocks.IsValidIndex(BlockIndex))
                                        {
                                            SourceScript->CaptureUndoSnapshot();

                                            SourceScript->Blocks[BlockIndex].Text = CachedEditText.ToString();

                                            if (OnScriptMutated.IsBound())
                                            {
                                                OnScriptMutated.Execute();
                                            }

                                            RebuildLayout();
                                            Invalidate(EInvalidateWidget::LayoutAndVolatility);
                                        }

                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })

                        ]
                ]
        ]
    );


    FSlateApplication::Get().AddModalWindow(
        EditWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );

    FSlateApplication::Get().SetKeyboardFocus(EditTextBox);

}

void SGASScriptPanel::OpenEditCharacterDialog(int32 BlockIndex)
{
    if (!SourceScript  || !SourceScript ->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    FGASBlock& Block = SourceScript ->Blocks[BlockIndex];

    TSharedRef<SWindow> EditWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Edit Character")))
        .ClientSize(FVector2D(400.f, 160.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    CachedEditText = FText::FromString(Block.Text);

    EditWindow->SetContent(
        SNew(SBorder)
        .Padding(8.f)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .FillHeight(1.f)
                [
                    SAssignNew(EditTextBox, SMultiLineEditableTextBox)
                        .Text(CachedEditText)
                        .OnTextChanged_Lambda([this](const FText& NewText)
                            {
                                CachedEditText = NewText;
                            })
                ]

            + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 8.f, 0.f, 0.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([this, EditWindow]()
                                    {
                                        OnEditCancelled();
                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("OK")))
                                .OnClicked_Lambda([this, EditWindow, BlockIndex]()
                                    {
                                        SourceScript->Blocks[BlockIndex].Text =
                                            CachedEditText.ToString().ToUpper();

                                        FGAS_PreProToolsEditorModule& Module =
                                            FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
                                                "GAS_PreProToolsEditor"
                                            );

                                        Module.MarkToolDirty();


                                        RebuildLayout();
                                        Invalidate(EInvalidateWidget::LayoutAndVolatility);

                                        // ----------------------------------------------------
                                        // If this Character was added with a Dialogue, open it next
                                        // ----------------------------------------------------
                                        if (PendingDialogueAfterCharacterIndex != INDEX_NONE)
                                        {
                                            const int32 DialogueIndex = PendingDialogueAfterCharacterIndex;
                                            PendingDialogueAfterCharacterIndex = INDEX_NONE;

                                            FTSTicker::GetCoreTicker().AddTicker(
                                                FTickerDelegate::CreateLambda(
                                                    [this, DialogueIndex](float)
                                                    {
                                                        OpenEditDialogueDialog(DialogueIndex);
                                                        return false; // one-shot
                                                    }
                                                )
                                            );
                                        }

                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(
        EditWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );

    FSlateApplication::Get().SetKeyboardFocus(EditTextBox);
}

void SGASScriptPanel::OpenAddBlockDialog(int32 InsertAfterParagraphIndex)
{
    if (!SourceScript)
    {
        return;
    }

    TSharedRef<SWindow> AddWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Add Block")))
        .ClientSize(FVector2D(300.f, 160.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    // 0 = Action, 1 = Character + Dialogue, 2 = Scene Heading
    TSharedPtr<int32> SelectedType = MakeShared<int32>(0);

    AddWindow->SetContent(
        SNew(SBorder)
        .Padding(8.f)
        [
            SNew(SVerticalBox)

                // ----------------------------------------------------
                // Block type selection
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 0.f, 0.f, 6.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Block Type")))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCheckBox)
                        .IsChecked_Lambda([SelectedType]()
                            {
                                return (*SelectedType == 0)
                                    ? ECheckBoxState::Checked
                                    : ECheckBoxState::Unchecked;
                            })
                        .OnCheckStateChanged_Lambda([SelectedType](ECheckBoxState)
                            {
                                *SelectedType = 0;
                            })
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Action")))
                        ]
                ]

            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCheckBox)
                        .IsChecked_Lambda([SelectedType]()
                            {
                                return (*SelectedType == 1)
                                    ? ECheckBoxState::Checked
                                    : ECheckBoxState::Unchecked;
                            })
                        .OnCheckStateChanged_Lambda([SelectedType](ECheckBoxState)
                            {
                                *SelectedType = 1;
                            })
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Character + Dialogue")))
                        ]
                ]
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SCheckBox)
                        .IsChecked_Lambda([SelectedType]()
                            {
                                return (*SelectedType == 2)
                                    ? ECheckBoxState::Checked
                                    : ECheckBoxState::Unchecked;
                            })
                        .OnCheckStateChanged_Lambda([SelectedType](ECheckBoxState)
                            {
                                *SelectedType = 2;
                            })
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Scene Heading")))
                        ]
                ]

            // ----------------------------------------------------
            // OK / Cancel
            // ----------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 8.f, 0.f, 0.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([AddWindow]()
                                    {
                                        AddWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("OK")))
                                .OnClicked_Lambda([this, AddWindow, InsertAfterParagraphIndex, SelectedType]()
                                    {
                                        InsertNewBlock(
                                            InsertAfterParagraphIndex,
                                            *SelectedType
                                        );

                                        AddWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })

                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(
        AddWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );
}

void SGASScriptPanel::OpenDialoguePreviewDialog(int32 BlockIndex)
{
    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    const FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    TSharedRef<SWindow> PreviewWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Dialogue Preview")))
        .ClientSize(FVector2D(500.f, 260.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    PreviewWindow->SetContent(
        SNew(SBorder)
        .Padding(8.f)
        [
            SNew(SVerticalBox)

                // ----------------------------------------------------
                // Info label
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 0.f, 0.f, 6.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Dialogue editing coming soon")))
                        .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.f))
                ]

                // ----------------------------------------------------
                // Dialogue text (read-only)
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .FillHeight(1.f)
                [
                    SNew(SMultiLineEditableTextBox)
                        .Text(FText::FromString(Block.Text))
                        .IsReadOnly(true)
                ]

                // ----------------------------------------------------
                // Close button
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 8.f, 0.f, 0.f)
                [
                    SNew(SButton)
                        .Text(FText::FromString(TEXT("Close")))
                        .OnClicked_Lambda([PreviewWindow]()
                            {
                                PreviewWindow->RequestDestroyWindow();
                                return FReply::Handled();
                            })
                ]
        ]
    );


    FSlateApplication::Get().AddModalWindow(
        PreviewWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );
}

void SGASScriptPanel::OpenEditDialogueDialog(int32 BlockIndex)
{
    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    TSharedRef<SWindow> EditWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Edit Dialogue")))
        .ClientSize(FVector2D(500.f, 280.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    CachedEditText = FText::FromString(Block.Text);

    EditWindow->SetContent(
        SNew(SBorder)
        .Padding(8.f)
        [
            SNew(SVerticalBox)

                // ----------------------------------------------------
                // Dialogue text
                // ----------------------------------------------------
                +SVerticalBox::Slot()
                .FillHeight(1.f)
                [
                    SAssignNew(EditTextBox, SMultiLineEditableTextBox)
                        .Text(CachedEditText)
                        .OnTextChanged_Lambda([this](const FText& NewText)
                            {
                                CachedEditText = NewText;
                            })
                        .OnKeyDownHandler_Lambda([this, EditWindow, BlockIndex](const FGeometry&, const FKeyEvent& KeyEvent)
                            {
                                // ESC = cancel
                                if (KeyEvent.GetKey() == EKeys::Escape)
                                {
                                    OnEditCancelled();
                                    EditWindow->RequestDestroyWindow();
                                    return FReply::Handled();
                                }


                                // CTRL + ENTER = accept
                                if (KeyEvent.GetKey() == EKeys::Enter && KeyEvent.IsControlDown())
                                {
                                    if (SourceScript && SourceScript->Blocks.IsValidIndex(BlockIndex))
                                    {
                                        SourceScript->Blocks[BlockIndex].Text =
                                            CachedEditText.ToString();

                                        FGAS_PreProToolsEditorModule& Module =
                                            FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
                                                "GAS_PreProToolsEditor"
                                            );

                                        Module.MarkToolDirty();


                                        RebuildLayout();
                                        Invalidate(EInvalidateWidget::LayoutAndVolatility);
                                    }

                                    EditWindow->RequestDestroyWindow();
                                    return FReply::Handled();
                                }

                                return FReply::Unhandled();
                            })
                ]

            // ----------------------------------------------------
            // Buttons
            // ----------------------------------------------------
            +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(0.f, 8.f, 0.f, 0.f)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([this, EditWindow]()
                                    {
                                        OnEditCancelled();
                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })

                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("OK")))
                                .OnClicked_Lambda([this, EditWindow, BlockIndex]()
                                    {
                                        if (SourceScript && SourceScript->Blocks.IsValidIndex(BlockIndex))
                                        {
                                            SourceScript->Blocks[BlockIndex].Text =
                                                CachedEditText.ToString();

                                            FGAS_PreProToolsEditorModule& Module =
                                                FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
                                                    "GAS_PreProToolsEditor"
                                                );

                                            Module.MarkToolDirty();


                                            RebuildLayout();
                                            Invalidate(EInvalidateWidget::LayoutAndVolatility);
                                        }

                                        EditWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(
        EditWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );

    FSlateApplication::Get().SetKeyboardFocus(EditTextBox);
}

void SGASScriptPanel::SetEditMode(bool bInEditMode)
{
    bEditMode = bInEditMode;

    if (bEditMode)
    {
        bAddMode = false;

        // 🔒 Clear any stale Add-mode rollback state
        PendingAddBlockCount = 0;
        PendingAddStartIndex = INDEX_NONE;
        PendingEditBlockIndex = INDEX_NONE;
    }

    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SGASScriptPanel::SetAddMode(bool bInAddMode)
{
    bAddMode = bInAddMode;

    // Add and Edit are mutually exclusive
    if (bAddMode)
    {
        bEditMode = false;
    }
}

void SGASScriptPanel::ResetEditorModes()
{
    // Core modes
    bEditMode = false;
    bAddMode = false;

    // Shot add / selection modes
    bShotAddArmed = false;
    bIsShotRangeDragging = false;
    bIsShotSelectMode = false;
    bIsDraggingShot = false;

    // Shot UI state
    HoveredShotMarkerId.Empty();
    SelectedShotMarkerId.Empty();
    HoveredShotParagraphIndex = INDEX_NONE;

    // Pending add/edit rollback safety
    PendingAddBlockCount = 0;
    PendingAddStartIndex = INDEX_NONE;
    PendingEditBlockIndex = INDEX_NONE;

    bNeedsRedraw = true;
    Invalidate(EInvalidateWidget::Paint);
}


void SGASScriptPanel::InsertNewBlock(
    int32 InsertAfterParagraphIndex,
    int32 BlockTypeChoice
)
{
    if (!SourceScript)
    {
        return;
    }

    // UNDO: capture state BEFORE mutation
    SourceScript->CaptureUndoSnapshot();

    const int32 InsertIndex = InsertAfterParagraphIndex + 1;

    if (BlockTypeChoice == 0)
    {
        FGASBlock NewBlock;
        NewBlock.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        NewBlock.Type = EGASBlockType::Action;
        NewBlock.Text = TEXT("");

        SourceScript->Blocks.Insert(NewBlock, InsertIndex);

        PendingAddStartIndex = InsertIndex;
        PendingAddBlockCount = 1;
        PendingEditBlockIndex = InsertIndex;
    }
    else if (BlockTypeChoice == 2) // Scene Heading
    {
        // Resolve current scene numbers (authoritative) BEFORE insertion
        const TMap<FString, FString> SceneNumberMap =
            FGASSceneNumberResolver::ResolveSceneNumbers(
                SourceScript->Blocks,
                SourceScript->SceneNumbering
            );

        FGASBlock SceneBlock;
        SceneBlock.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        SceneBlock.Type = EGASBlockType::SceneHeading;
        SceneBlock.Text = TEXT("NEW SCENE");

        // Find nearest previous SceneHeading (base)
        FString BaseSceneLabel;

        for (int32 i = InsertAfterParagraphIndex; i >= 0; --i)
        {
            if (SourceScript->Blocks.IsValidIndex(i) &&
                SourceScript->Blocks[i].Type == EGASBlockType::SceneHeading)
            {
                const FString PrevId = SourceScript->Blocks[i].Id;
                BaseSceneLabel = SceneNumberMap.FindRef(PrevId);
                break;
            }
        }

        if (BaseSceneLabel.IsEmpty())
        {
            BaseSceneLabel = TEXT("INS");
        }

        // Count existing inserts under this base label
        int32 InsertCount = 1;
        for (const FGASBlock& Block : SourceScript->Blocks)
        {
            if (Block.Type != EGASBlockType::SceneHeading)
                continue;

            const FString Existing = Block.Metadata.FindRef(TEXT("SceneNumber"));
            if (Existing.StartsWith(BaseSceneLabel + TEXT("X")))
            {
                InsertCount++;
            }
        }

        const FString InsertLabel =
            FString::Printf(TEXT("%sX%d"), *BaseSceneLabel, InsertCount);

        // Lock this number so it never renumbers anything
        SceneBlock.Metadata.Add(TEXT("SceneNumber"), InsertLabel);
        SceneBlock.Metadata.Add(TEXT("SceneLocked"), TEXT("1"));

        SourceScript->Blocks.Insert(SceneBlock, InsertIndex);

        PendingAddStartIndex = InsertIndex;
        PendingAddBlockCount = 1;
        PendingEditBlockIndex = InsertIndex;
    }


    else
    {
        FGASBlock CharacterBlock;
        CharacterBlock.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        CharacterBlock.Type = EGASBlockType::Character;
        CharacterBlock.Text = TEXT("");

        FGASBlock DialogueBlock;
        DialogueBlock.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        DialogueBlock.Type = EGASBlockType::Dialogue;
        DialogueBlock.Text = TEXT("");

        SourceScript->Blocks.Insert(CharacterBlock, InsertIndex);
        SourceScript->Blocks.Insert(DialogueBlock, InsertIndex + 1);

        PendingAddStartIndex = InsertIndex;
        PendingAddBlockCount = 2;
        PendingEditBlockIndex = InsertIndex;
        PendingDialogueAfterCharacterIndex = InsertIndex + 1;
    }


    // Mark dirty
    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.MarkToolDirty();


    RebuildLayout();
    if (OnScriptMutated.IsBound())
    {
        OnScriptMutated.Execute();
    }

    Invalidate(EInvalidateWidget::LayoutAndVolatility);

    // Immediately open edit dialog for the newly inserted block
    if (PendingEditBlockIndex != INDEX_NONE)
    {
        TryEditParagraph(PendingEditBlockIndex);
        PendingEditBlockIndex = INDEX_NONE;
    }
}

void SGASScriptPanel::OnEditCancelled()
{
    if (!SourceScript)
    {
        return;
    }

    // 🔒 ONLY rollback if we are CURRENTLY in Add Mode
    if (bAddMode &&
        PendingAddBlockCount > 0 &&
        PendingAddStartIndex != INDEX_NONE &&
        SourceScript->Blocks.IsValidIndex(PendingAddStartIndex))
    {
        SourceScript->Blocks.RemoveAt(
            PendingAddStartIndex,
            PendingAddBlockCount,
            /*bAllowShrinking=*/false
        );
    }

    PendingAddBlockCount = 0;
    PendingAddStartIndex = INDEX_NONE;
    PendingEditBlockIndex = INDEX_NONE;

    RebuildLayout();
    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}


void SGASScriptPanel::OpenDeleteBlockDialog(int32 BlockIndex)
{
    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    FString BlockLabel;
    switch (Block.Type)
    {
    case EGASBlockType::Action:     BlockLabel = TEXT("Action"); break;
    case EGASBlockType::Character:  BlockLabel = TEXT("Character"); break;
    case EGASBlockType::Dialogue:   BlockLabel = TEXT("Dialogue"); break;
    default:                        BlockLabel = TEXT("Block"); break;
    }

    TSharedRef<SWindow> ConfirmWindow =
        SNew(SWindow)
        .Title(FText::FromString(TEXT("Delete Block")))
        .ClientSize(FVector2D(360.f, 140.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    ConfirmWindow->SetContent(
        SNew(SBorder)
        .Padding(12.f)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.f, 0.f, 0.f, 10.f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(
                            FString::Printf(TEXT("Delete this %s block?"), *BlockLabel)
                        ))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([ConfirmWindow]()
                                    {
                                        ConfirmWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(4.f)
                        [
                            SNew(SButton)
                                .Text(FText::FromString(TEXT("Delete")))
                                .OnClicked_Lambda([this, ConfirmWindow, BlockIndex]()
                                    {
                                        DeleteBlock(BlockIndex);
                                        ConfirmWindow->RequestDestroyWindow();
                                        return FReply::Handled();
                                    })
                        ]
                ]
        ]
    );

    FSlateApplication::Get().AddModalWindow(
        ConfirmWindow,
        FSlateApplication::Get().GetActiveTopLevelWindow()
    );
}

void SGASScriptPanel::DeleteBlock(int32 BlockIndex)
{
    if (!SourceScript || !SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        return;
    }

    // UNDO: capture state BEFORE deletion
    SourceScript->CaptureUndoSnapshot();


    // If deleting a Character, also delete its Dialogue if present
    if (SourceScript->Blocks[BlockIndex].Type == EGASBlockType::Character)
    {
        if (SourceScript->Blocks.IsValidIndex(BlockIndex + 1) &&
            SourceScript->Blocks[BlockIndex + 1].Type == EGASBlockType::Dialogue)
        {
            SourceScript->Blocks.RemoveAt(BlockIndex, 2);
        }
        else
        {
            SourceScript->Blocks.RemoveAt(BlockIndex);
        }
    }
    // If deleting Dialogue, check if previous is Character
    else if (SourceScript->Blocks[BlockIndex].Type == EGASBlockType::Dialogue)
    {
        if (BlockIndex > 0 &&
            SourceScript->Blocks[BlockIndex - 1].Type == EGASBlockType::Character)
        {
            SourceScript->Blocks.RemoveAt(BlockIndex - 1, 2);
        }
        else
        {
            SourceScript->Blocks.RemoveAt(BlockIndex);
        }
    }
    else
    {
        // Action or other block
        SourceScript->Blocks.RemoveAt(BlockIndex);
    }

    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.MarkToolDirty();


    RebuildLayout();
    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

FReply SGASScriptPanel::OnKeyDown(
    const FGeometry& MyGeometry,
    const FKeyEvent& InKeyEvent
)
{
    // CTRL + Z → Undo
    if (InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::Z)
    {
        if (SourceScript && SourceScript->CanUndo())
        {
            SourceScript->Undo();
            RebuildLayout();
            Invalidate(EInvalidateWidget::LayoutAndVolatility);
            return FReply::Handled();
        }
    }

    // CTRL + Y → Redo
    if (InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::Y)
    {
        if (SourceScript && SourceScript->CanRedo())
        {
            SourceScript->Redo();
            RebuildLayout();
            Invalidate(EInvalidateWidget::LayoutAndVolatility);
            return FReply::Handled();
        }
    }

    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SGASScriptPanel::ScrollToParagraph(int32 ParagraphIndex)
{
    if (!RenderedParagraphs.IsValidIndex(ParagraphIndex))
    {
        return;
    }

    PendingScrollParagraph = ParagraphIndex;
    Invalidate(EInvalidateWidget::Paint);
}


void SGASScriptPanel::ScrollToBlockId(const FString& BlockId)
{
    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        if (RenderedParagraphs[i].BlockId == BlockId)
        {
            PendingScrollParagraph = i;
            Invalidate(EInvalidateWidget::Paint);
            return;
        }
    }
}


FString SGASScriptPanel::GetSelectedBlockId() const
{
    if (!RenderedParagraphs.IsValidIndex(SelectedParagraphIndex))
    {
        return FString();
    }

    return RenderedParagraphs[SelectedParagraphIndex].BlockId;
}




// =============================================================
//  PAINT — Draw text, page breaks, preview line
// =============================================================
int32 SGASScriptPanel::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{

    // Track total height
    CachedTotalHeight = 0.f;
    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        CachedTotalHeight = FMath::Max(
            CachedTotalHeight,
            P.TopY + P.Height
        );
    }

    // ------------------------------------------------------------
    // Apply pending scroll-to-paragraph (scene jump)
    // MUST happen BEFORE drawing
    // ------------------------------------------------------------
    if (PendingScrollParagraph != INDEX_NONE &&
        RenderedParagraphs.IsValidIndex(PendingScrollParagraph))
    {
        const FRenderedParagraph& Para =
            RenderedParagraphs[PendingScrollParagraph];

        const float ViewportHeight =
            MyCullingRect.GetSize().Y;

        const float MaxScroll =
            FMath::Max(0.f, CachedTotalHeight - ViewportHeight);

        constexpr float SceneScrollPadding = 24.f;

        ScrollOffsetY = FMath::Clamp(
            Para.TopY - ScriptFormat::MarginTop - SceneScrollPadding,
            0.f,
            MaxScroll
        );

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("SCROLL JUMP → Para=%d TopY=%.1f Offset=%.1f"),
            PendingScrollParagraph,
            Para.TopY,
            ScrollOffsetY
        );

        PendingScrollParagraph = INDEX_NONE;
    }


    const FSlateFontInfo FontInfo =
        FGAS_PreProToolsStyle::Get().GetFontStyle("GAS.ScriptFont");

    const TSharedRef<FSlateFontMeasure> FontMeasureService =
        FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    ShotHitRects.Reset();


    // ------------------------------------------------------------
    // Build Scene → Shot stack map
    // ------------------------------------------------------------
    TMap<FString, TArray<const FGASMarker*>> SceneShotMap;

    if (SourceScript)
    {
        for (const FGASMarker& Marker : SourceScript->Markers)
        {
            if (Marker.MarkerType != EGASMarkerType::ShotMarker)
                continue;

            SceneShotMap
                .FindOrAdd(Marker.BlockId)
                .Add(&Marker);
        }

        for (auto& Pair : SceneShotMap)
        {
            Pair.Value.Sort(
                [](const FGASMarker& A, const FGASMarker& B)
                {
                    return A.ShotIndex < B.ShotIndex;
                }
            );
        }
    }

    // ------------------------------------------------------------
    // Shot Range Drag Preview
    // ------------------------------------------------------------
    if (bIsShotRangeDragging &&
        RenderedParagraphs.IsValidIndex(ShotRangeStartParagraph) &&
        RenderedParagraphs.IsValidIndex(ShotRangeCurrentParagraph))
    {
        const int32 Start =
            FMath::Min(ShotRangeStartParagraph, ShotRangeCurrentParagraph);

        const int32 End =
            FMath::Max(ShotRangeStartParagraph, ShotRangeCurrentParagraph);

        const float TopY =
            RenderedParagraphs[Start].TopY - ScrollOffsetY;

        const float BottomY =
            RenderedParagraphs[End].TopY +
            RenderedParagraphs[End].Height -
            ScrollOffsetY;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 30,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(0.f, TopY),
                FVector2D(
                    AllottedGeometry.GetLocalSize().X,
                    BottomY - TopY
                )
            ),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            FLinearColor(0.25f, 0.6f, 1.f, 0.18f) // translucent blue
        );
    }

    for (int32 i = 0; i < RenderedParagraphs.Num(); i++)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];


        // ------------------------------------------------------------
        // Draw selection highlight (Edit Mode only)
        // ------------------------------------------------------------
        if (bEditMode && i == SelectedParagraphIndex)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(0.f, P.TopY - ScrollOffsetY),
                    FVector2D(AllottedGeometry.GetLocalSize().X, P.Height)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor(0.15f, 0.35f, 0.6f, 0.25f)
            );
        }


        // ------------------------------------------------------------
        // Draw SCENE NUMBER (optional, margin only — no layout impact)
        // ------------------------------------------------------------
        if (bShowSceneNumbers &&
            P.BlockType == EGASBlockType::SceneHeading)


        {
            if (!P.SceneLabel.IsEmpty())
            {
                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    LayerId,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(
                            ScriptFormat::PageLeft + P.IndentLeft - 50.f,
                            P.TopY - ScrollOffsetY
                        ),
                        FVector2D::UnitVector
                    ),
                    FText::FromString(P.SceneLabel),
                    FontInfo,
                    ESlateDrawEffect::None,
                    FLinearColor(1.f, 0.f, 0.f, 1.f)
                );
            }

        }

        // ------------------------------------------------------------
        // Draw stacked SHOT MARKERS under Scene Heading
        // ------------------------------------------------------------
        if (P.BlockType == EGASBlockType::SceneHeading &&
            SourceScript)
        {
            const TArray<const FGASMarker*>* Shots =
                SceneShotMap.Find(P.BlockId);

            if (Shots && Shots->Num() > 0)
            {
                for (const FGASMarker* Shot : *Shots)
                {
                    int32 ShotParaIndex = Shot->ParagraphIndex;

                    // Fallback for legacy / derived shots
                    if (!RenderedParagraphs.IsValidIndex(ShotParaIndex))
                    {
                        for (int32 p = 0; p < RenderedParagraphs.Num(); ++p)
                        {
                            if (RenderedParagraphs[p].BlockId == Shot->BlockId)
                            {
                                ShotParaIndex = p;
                                break;
                            }
                        }
                    }

                    // ------------------------------------------------------------
                    // TEST 10B — Visual Y comes ONLY from stored ScreenY
                    // ------------------------------------------------------------
                    if (Shot->ScreenY < 0.f)
                    {
                        continue;
                    }

                    const float PillY =
                        Shot->ScreenY - ScrollOffsetY;

                    const float PillX =
                        Shot->ScreenX >= 0.f
                        ? Shot->ScreenX
                        : (ScriptFormat::PageLeft + P.IndentLeft);

                    const bool bIsHovered =
                        (HoveredShotMarkerId == Shot->Id);

                    const bool bIsSelected =
                        (SelectedShotMarkerId == Shot->Id);

                    const FString* ShotType =
                        Shot->Metadata.Find(TEXT("Type"));

                    const bool bIsShotDefined =
                        (ShotType && !ShotType->IsEmpty() && *ShotType != TEXT("—"));

                    const FLinearColor ShotLineColor =
                        bIsShotDefined
                        ? FLinearColor(0.25f, 0.6f, 1.f, 0.9f)      // BLUE
                        : FLinearColor(0.95f, 0.85f, 0.25f, 0.95f); // YELLOW


                    // ------------------------------------------------------------
                    // Draw PERMANENT shot range line (stored)
                    // ------------------------------------------------------------
                    if (Shot->ShotLineStartY >= 0.f && Shot->ShotLineEndY >= 0.f)
                    {
                        const float LineX =
                            Shot->ScreenX + (ShotPillWidth * 0.5f);


                        // Line starts at bottom of pill
                        const float LineStartY =
                            Shot->ScreenY + ShotPillHeight;

                        const float TopY =
                            FMath::Min(LineStartY, Shot->ShotLineEndY) - ScrollOffsetY;

                        const float BottomY =
                            FMath::Max(LineStartY, Shot->ShotLineEndY) - ScrollOffsetY;


                        FSlateDrawElement::MakeBox(
                            OutDrawElements,
                            LayerId + 1, // below pill, above text
                            AllottedGeometry.ToPaintGeometry(
                                FVector2D(LineX - 1.f, TopY),
                                FVector2D(2.f, BottomY - TopY)
                            ),
                            FCoreStyle::Get().GetBrush("WhiteBrush"),
                            ESlateDrawEffect::None,
                            ShotLineColor
                        );

                        // Horizontal end-cap (≈ 4 characters wide)
                        const float CharWidth =
                            FontMeasureService->Measure(TEXT("M"), FontInfo).X;

                        const float CapHalfWidth = (CharWidth * 4.f) * 0.5f;

                        FSlateDrawElement::MakeBox(
                            OutDrawElements,
                            LayerId + 1,
                            AllottedGeometry.ToPaintGeometry(
                                FVector2D(LineX - CapHalfWidth, BottomY - 1.f),
                                FVector2D(CharWidth * 4.f, 2.f)
                            ),
                            FCoreStyle::Get().GetBrush("WhiteBrush"),
                            ESlateDrawEffect::None,
                            ShotLineColor
                        );


                    }

                    // ------------------------------------------------------------
                    // Shot tail resize ghost (ALT + drag)
                    // ------------------------------------------------------------
                    if (bIsResizingShotTail &&
                        ResizingShotMarkerId == Shot->Id &&
                        ShotTailGhostY >= 0.f)
                    {
                        const float LineX =
                            Shot->ScreenX + (ShotPillWidth * 0.5f);

                        const float TopY =
                            FMath::Min(Shot->ShotLineStartY, ShotTailGhostY) - ScrollOffsetY;

                        const float BottomY =
                            FMath::Max(Shot->ShotLineStartY, ShotTailGhostY) - ScrollOffsetY;

                        FSlateDrawElement::MakeBox(
                            OutDrawElements,
                            LayerId + 5,
                            AllottedGeometry.ToPaintGeometry(
                                FVector2D(LineX - 1.f, TopY),
                                FVector2D(2.f, BottomY - TopY)
                            ),
                            FCoreStyle::Get().GetBrush("WhiteBrush"),
                            ESlateDrawEffect::None,
                            FLinearColor(0.25f, 0.6f, 1.f, 0.55f)
                        );
                    }


                    // ------------------------------------------------------------
                    // Shot tail resize ghost HANDLE
                    // ------------------------------------------------------------
                    if (bIsResizingShotTail &&
                        ResizingShotMarkerId == Shot->Id &&
                        ShotTailGhostY >= 0.f)
                    {
                        const float LineX =
                            Shot->ScreenX + (ShotPillWidth * 0.5f);

                        constexpr float HandleHalfWidth = 8.f;
                        constexpr float HandleHalfHeight = 3.f;

                        const float HandleY =
                            ShotTailGhostY - ScrollOffsetY;

                        FSlateDrawElement::MakeBox(
                            OutDrawElements,
                            LayerId + 6,
                            AllottedGeometry.ToPaintGeometry(
                                FVector2D(
                                    LineX - HandleHalfWidth,
                                    HandleY - HandleHalfHeight
                                ),
                                FVector2D(
                                    HandleHalfWidth * 2.f,
                                    HandleHalfHeight * 2.f
                                )
                            ),
                            FCoreStyle::Get().GetBrush("WhiteBrush"),
                            ESlateDrawEffect::None,
                            FLinearColor(0.25f, 0.6f, 1.f, 0.9f)
                        );
                    }


                    // Background pill
                    const FLinearColor ShotFillColor =
                        bIsSelected
                        ? FLinearColor(0.2f, 0.8f, 0.35f, 1.0f)   // 🟢 SELECTED
                        : bIsHovered
                        ? ShotLineColor.CopyWithNewOpacity(0.85f)
                        : ShotLineColor.CopyWithNewOpacity(1.0f);

                    FSlateDrawElement::MakeBox(
                        OutDrawElements,
                        LayerId + 2,
                        AllottedGeometry.ToPaintGeometry(
                            FVector2D(PillX, PillY),
                            FVector2D(48.f, 18.f)
                        ),
                        FCoreStyle::Get().GetBrush("WhiteBrush"),
                        ESlateDrawEffect::None,
                        ShotFillColor
                    );



                    // ------------------------------------------------------------
                    // Shot label — computed dynamically (NO cached ShotId)
                    // ------------------------------------------------------------
                    const FString ShotLabel =
                        ResolveShotDisplayLabel(
                            *SourceScript,
                            /*SceneBlockIndex=*/INDEX_NONE,
                            Shot->ShotIndex - 1
                        );



                    const FVector2D TextSize =
                        FontMeasureService->Measure(ShotLabel, FontInfo);

                    FSlateDrawElement::MakeText(
                        OutDrawElements,
                        LayerId + 3,
                        AllottedGeometry.ToPaintGeometry(
                            FVector2D(
                                PillX + (ShotPillWidth - TextSize.X) * 0.5f,
                                PillY + (ShotPillHeight - TextSize.Y) * 0.5f
                            ),
                            FVector2D::UnitVector
                        ),
                        ShotLabel,
                        FontInfo,
                        ESlateDrawEffect::None,
                        bIsShotDefined
                        ? FLinearColor(1.f, 1.f, 1.f, 1.f)   // WHITE on blue
                        : FLinearColor(0.f, 0.f, 0.f, 1.f)   // BLACK on yellow

                    );



                    // Hit rect
                    FShotHitRect Hit;
                    Hit.MarkerId = Shot->Id;
                    Hit.Rect = FSlateRect(
                        PillX,
                        PillY,
                        PillX + 48.f,
                        PillY + 18.f
                    );

                    ShotHitRects.Add(Hit);
                }

            }
        }



        // ------------------------------------------------------------
        // Shot drag ghost
        // ------------------------------------------------------------
        if (bIsDraggingShot && ShotDragGhostX >= 0.f && ShotDragGhostY >= 0.f)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 50,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(
                        ShotDragGhostX,
                        ShotDragGhostY - ScrollOffsetY
                    ),
                    FVector2D(ShotPillWidth, ShotPillHeight)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor(0.25f, 0.6f, 1.f, 0.35f)
            );
        }



        // ------------------------------------------------------------
        // Draw PAGE BREAK marker (derived, visual only)
        // ------------------------------------------------------------

        if (P.bStartsPage)
        {
            const FString PageText =
                FString::Printf(TEXT("PAGE %d"), P.PageNumber);

            const float MarkerY = P.TopY - ScrollOffsetY;
            const float MarkerX = 10.f; // force "PAGE" to be visible (no clipping)

            //const float MarkerX = ScriptFormat::PageLeft * 0.35f;

            const bool bIsHovered =
                bEditMode &&
                (HoveredPageBreakIndex != INDEX_NONE) &&
                SourceScript &&
                SourceScript->UserPageBreaks.IsValidIndex(HoveredPageBreakIndex) &&
                SourceScript->UserPageBreaks[HoveredPageBreakIndex].AfterBlockId
                == P.BlockId;

            const FLinearColor TextColor =
                bIsHovered
                ? FLinearColor(1.0f, 1.0f, 0.6f, 1.f)
                : FLinearColor(1.0f, 0.85f, 0.2f, 1.f);

            if (bIsHovered)
            {
                const float RuleThickness = 3.0f;

                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    LayerId + 0,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(0.f, MarkerY + ScriptFormat::LineHeight * 0.4f),
                        FVector2D(AllottedGeometry.GetLocalSize().X, RuleThickness)
                    ),
                    FCoreStyle::Get().GetBrush("WhiteBrush"),
                    ESlateDrawEffect::None,
                    FLinearColor(1.0f, 0.9f, 0.3f, 0.8f)
                );
            }

            FSlateDrawElement::MakeText(
                OutDrawElements,
                LayerId + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(MarkerX, MarkerY),
                    FVector2D::UnitVector
                ),
                PageText,
                FontInfo,
                ESlateDrawEffect::None,
                TextColor
            );
        }

        // ----------------------------------------------
        // Draw lines at TRUE layout coordinates
        // ----------------------------------------------
        float LineY = P.TopY - ScrollOffsetY;


        for (const FString& Ln : P.Lines)
        {

            FSlateDrawElement::MakeText(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(ScriptFormat::PageLeft + P.IndentLeft, LineY),
                    FVector2D::UnitVector
                ),
                FText::FromString(Ln),
                FontInfo,
                ESlateDrawEffect::None,
                FLinearColor(0.35f, 0.35f, 0.35f, 1.f)
            );

            LineY += ScriptFormat::LineHeight;
        }

        // ------------------------------------------------------------
        // Draw stacked shots for this SceneHeading
        // ------------------------------------------------------------
        if (P.BlockType == EGASBlockType::SceneHeading)
        {
            const TArray<const FGASMarker*>* ShotsForScene =
                SceneShotMap.Find(P.BlockId);

            if (ShotsForScene)
            {
                const float ShotRowHeight = 18.f;
                float ShotY = LineY + 4.f; // below last scene line

                for (const FGASMarker* Shot : *ShotsForScene)
                {

                    ShotY += ShotRowHeight;
                }
            }
        }


    }

    // ------------------------------------------------------------
    // Draw page break drag preview
    // ------------------------------------------------------------
    if ((bIsDraggingPageBreak || bAnimatingPageBreak) && bEditMode)
    {
        const float LineThickness = 2.0f;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 50,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(0.f, DragPreviewY - ScrollOffsetY),
                FVector2D(AllottedGeometry.GetLocalSize().X, LineThickness)
            ),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            FLinearColor(1.0f, 0.85f, 0.2f, 0.9f)
        );
    }

    // ------------------------------------------------------------
    // Shot Add Cursor Icon + LIVE PREVIEW LINE
    // ------------------------------------------------------------
    if (bShotAddArmed)
    {
        const float IconY = ShotGhostY - ScrollOffsetY;

        const FVector2D IconPos(
            10.f,          // left gutter
            IconY - 10.f   // center icon on cursor
        );

        // Camera icon
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 20,
            AllottedGeometry.ToPaintGeometry(
                IconPos,
                FVector2D(20.f, 20.f)
            ),
            FGAS_PreProToolsStyle::Get().GetBrush("GAS.CameraIcon"),
            ESlateDrawEffect::None,
            FLinearColor(0.25f, 0.6f, 1.f, 0.9f)
        );

        // --------------------------------------------------------
        // Live shot line preview (while LMB held)
        // --------------------------------------------------------
        if (bIsShotRangeDragging)
        {
            const float LineX = ShotRangeStartX;
            const float TopY =
                FMath::Min(ShotRangeStartY, ShotGhostY) - ScrollOffsetY;
            const float BottomY =
                FMath::Max(ShotRangeStartY, ShotGhostY) - ScrollOffsetY;

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId + 19,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(LineX - 1.f, TopY),
                    FVector2D(2.f, BottomY - TopY)
                ),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor(0.25f, 0.6f, 1.f, 0.6f)
            );
        }
    }




    bNeedsRedraw = false;

    return LayerId + 10;
}


int32 SGASScriptPanel::HitTestParagraph(float ScriptY) const
{
    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        if (ScriptY >= P.TopY && ScriptY < (P.TopY + P.Height))
        {
            return i;
        }
    }

    return INDEX_NONE;
}


void SGASScriptPanel::CommitShotAtParagraph(
    int32 SceneParaIndex,
    int32 InsertIndex,
    float ShotStartScriptY,
    float ShotEndScriptY,
    float CommitX
)
{
    if (!SourceScript)
        return;

    if (!RenderedParagraphs.IsValidIndex(SceneParaIndex))
        return;

    const FRenderedParagraph& ScenePara =
        RenderedParagraphs[SceneParaIndex];

    if (ScenePara.BlockType != EGASBlockType::SceneHeading)
        return;

    const FString& SceneBlockId = ScenePara.BlockId;

    const int32 NewShotIndex = InsertIndex + 1;

    // Shift existing shots
    for (FGASMarker& M : SourceScript->Markers)
    {
        if (M.MarkerType == EGASMarkerType::ShotMarker &&
            M.BlockId == SceneBlockId &&
            M.ShotIndex >= NewShotIndex)
        {
            M.ShotIndex++;
        }
    }

    FGASMarker NewMarker;
    NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    NewMarker.MarkerType = EGASMarkerType::ShotMarker;
    NewMarker.BlockId = SceneBlockId;
    NewMarker.ShotOrigin = EGASShotOrigin::User;

    // INTERNAL ordering ONLY
    NewMarker.ShotIndex = NewShotIndex;

    // Position
    NewMarker.ScreenX = CommitX - (ShotPillWidth * 0.5f);
    NewMarker.ScreenY = ShotStartScriptY - ShotPillHeight;

    NewMarker.ShotLineStartY = NewMarker.ScreenY;
    NewMarker.ShotLineEndY = ShotEndScriptY;

    const int32 ParaIndex = HitTestParagraph(ShotStartScriptY);
    NewMarker.ParagraphIndex = ParaIndex;

    int32 LineIndex = 0;
    if (RenderedParagraphs.IsValidIndex(ParaIndex))
    {
        const FRenderedParagraph& P = RenderedParagraphs[ParaIndex];
        LineIndex = FMath::Clamp(
            FMath::FloorToInt((ShotStartScriptY - P.TopY) / ScriptFormat::LineHeight),
            0,
            FMath::Max(0, P.Lines.Num() - 1)
        );
    }

    NewMarker.LineIndex = LineIndex;

    SourceScript->Markers.Add(NewMarker);


    OnScriptMutated.ExecuteIfBound();
    bNeedsRedraw = true;
}




FString SGASScriptPanel::GetSourceTextForBlock(const FString& BlockId) const
{
    if (!SourceScript)
        return FString();

    for (const FGASBlock& Block : SourceScript->Blocks)
    {
        if (Block.Id == BlockId)
        {
            return Block.Text;
        }
    }

    return FString();
}


void SGASScriptPanel::SetShowSceneNumbers(bool bInShow)
{
    bShowSceneNumbers = bInShow;

    // Scene numbers are visual-only but depend on layout
    RebuildLayout();
    Invalidate(EInvalidateWidget::LayoutAndVolatility);
}


void SGASScriptPanel::SetShotMarkers(const TArray<FGASMarker>& Markers)
{
    ShotMarkers = Markers;
    bNeedsRedraw = true;
    Invalidate(EInvalidateWidget::Paint);

}


// ------------------------------------------------------------
// MOUSE CONTROLS
// ------------------------------------------------------------

FReply SGASScriptPanel::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    const bool bLeftMouse =
        MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;

    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const float MouseY = LocalPos.Y + ScrollOffsetY;

    // ============================================================
    // SHOT DELETE (RMB on pill) — CONFIRM
    // ============================================================
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        const FString HitShotId = HitTestShot(LocalPos);
        if (!HitShotId.IsEmpty() && SourceScript)
        {
            // Capture info up front
            FString SceneBlockId;
            int32 DeletedShotIndex = INDEX_NONE;

            for (const FGASMarker& M : SourceScript->Markers)
            {
                if (M.Id == HitShotId &&
                    M.MarkerType == EGASMarkerType::ShotMarker)
                {
                    SceneBlockId = M.BlockId;
                    DeletedShotIndex = M.ShotIndex;
                    break;
                }
            }

            TSharedRef<SWindow> ConfirmWindow =
                SNew(SWindow)
                .Title(FText::FromString(TEXT("Delete Shot")))
                .ClientSize(FVector2D(360.f, 140.f))
                .SupportsMinimize(false)
                .SupportsMaximize(false);

            ConfirmWindow->SetContent(
                SNew(SBorder)
                .Padding(12.f)
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 0.f, 0.f, 10.f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Delete this shot marker?")))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Right)
                        [
                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(4.f)
                                [
                                    SNew(SButton)
                                        .Text(FText::FromString(TEXT("Cancel")))
                                        .OnClicked_Lambda([ConfirmWindow]()
                                            {
                                                ConfirmWindow->RequestDestroyWindow();
                                                return FReply::Handled();
                                            })
                                ]

                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(4.f)
                                [
                                    SNew(SButton)
                                        .Text(FText::FromString(TEXT("Delete")))
                                        .OnClicked_Lambda([this, ConfirmWindow, HitShotId, SceneBlockId, DeletedShotIndex]()
                                            {
                                                // UNDO SNAPSHOT — before mutation
                                                SourceScript->CaptureUndoSnapshot();

                                                // Remove the shot
                                                SourceScript->Markers.RemoveAll(
                                                    [&](const FGASMarker& M)
                                                    {
                                                        return M.Id == HitShotId;
                                                    }
                                                );

                                                // Renumber remaining shots in the scene
                                                if (!SceneBlockId.IsEmpty() && DeletedShotIndex != INDEX_NONE)
                                                {
                                                    for (FGASMarker& M : SourceScript->Markers)
                                                    {
                                                        if (M.MarkerType == EGASMarkerType::ShotMarker &&
                                                            M.BlockId == SceneBlockId &&
                                                            M.ShotIndex > DeletedShotIndex)
                                                        {
                                                            M.ShotIndex--;
                                                        }
                                                    }
                                                }

                                                SelectedShotMarkerId.Empty();
                                                HoveredShotMarkerId.Empty();

                                                OnScriptMutated.ExecuteIfBound();
                                                bNeedsRedraw = true;

                                                ConfirmWindow->RequestDestroyWindow();
                                                return FReply::Handled();
                                            })

                                ]
                        ]
                ]
            );

            FSlateApplication::Get().AddModalWindow(
                ConfirmWindow,
                FSlateApplication::Get().GetActiveTopLevelWindow()
            );

            return FReply::Handled();
        }
    }



    // ============================================================
    // SHOT TAIL RESIZE START (ALT + LMB)
    // ============================================================
    if (bLeftMouse && MouseEvent.IsAltDown())
    {
        const FString HitTailId = HitTestShotTail(LocalPos);
        if (!HitTailId.IsEmpty())
        {
            bIsResizingShotTail = true;
            ResizingShotMarkerId = HitTailId;

            for (const FGASMarker& M : SourceScript->Markers)
            {
                if (M.Id == HitTailId)
                {
                    ShotTailGhostY = M.ShotLineEndY;
                    break;
                }
            }

            return FReply::Handled().CaptureMouse(AsShared());
        }
    }


    // ============================================================
    // 1️⃣ SHOT ICON HIT TEST (ALT + LMB DRAG)
    // ============================================================
    if (bLeftMouse && MouseEvent.IsAltDown())

    {
        const FString HitShotId = HitTestShot(LocalPos);
        if (!HitShotId.IsEmpty())
        {
            bIsDraggingShot = true;
            DraggingShotMarkerId = HitShotId;

            for (const FGASMarker& M : SourceScript->Markers)
            {
                if (M.Id == HitShotId)
                {
                    ShotDragOffset = FVector2D(
                        LocalPos.X - M.ScreenX,
                        (LocalPos.Y + ScrollOffsetY) - M.ScreenY
                    );
                    ShotDragGhostX = M.ScreenX;
                    ShotDragGhostY = M.ScreenY;
                    break;
                }
            }

            return FReply::Handled().CaptureMouse(AsShared());
        }
    }




    // ------------------------------------------------------------
    // Shot Range Drag START
    // ------------------------------------------------------------
    if (bShotAddArmed &&
        MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const float ScriptY = LocalPos.Y + ScrollOffsetY;
        const int32 ParaIndex = HitTestParagraph(ScriptY);

        if (ParaIndex != INDEX_NONE)
        {
            bIsShotRangeDragging = true;
            ShotRangeStartParagraph = ParaIndex;
            ShotRangeStartX = LocalPos.X;
            ShotRangeStartY = ScriptY;

            ShotRangeCurrentParagraph = ParaIndex;

            return FReply::Handled().CaptureMouse(AsShared());
        }
    }






    // ============================================================
    // 2️⃣ SHOT ADD COMMIT (INSERT AT GHOST LINE)
    // ============================================================
    if (bShotAddArmed && bLeftMouse)
    {
        if (!SourceScript)
        {
            return FReply::Handled();
        }

        // Find the owning SceneHeading for this Y (works over pills too)
        const int32 SceneParaIndex = ResolveSceneIndexAtY(LocalPos.Y);

        // ------------------------------------------------------------
        // BLOCK SHOT ADD DIRECTLY UNDER SCENE HEADING
        // ------------------------------------------------------------
        if (RenderedParagraphs.IsValidIndex(SceneParaIndex))
        {
            const FRenderedParagraph& ScenePara = RenderedParagraphs[SceneParaIndex];

            const float ScriptY = LocalPos.Y + ScrollOffsetY;

            if (ScriptY <= ScenePara.TopY + ScenePara.Height + ScriptFormat::LineHeight)
            {
                return FReply::Handled();
            }
        }



        if (SceneParaIndex == INDEX_NONE ||
            !RenderedParagraphs.IsValidIndex(SceneParaIndex) ||
            RenderedParagraphs[SceneParaIndex].BlockType != EGASBlockType::SceneHeading)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShotAdd] Click rejected: no owning SceneHeading"));
            return FReply::Handled();
        }

        const FString SceneBlockId = RenderedParagraphs[SceneParaIndex].BlockId;
        if (SceneBlockId.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShotAdd] Click rejected: Scene has no BlockId"));
            return FReply::Handled();
        }

        // Script-space Y (NOT local)
        const float ScriptY = LocalPos.Y + ScrollOffsetY;

        // Resolve exact script line the user clicked
        int32 ClickedParaIndex = HitTestParagraph(ScriptY);
        int32 ClickedLineIndex = 0;

        if (RenderedParagraphs.IsValidIndex(ClickedParaIndex))
        {
            const FRenderedParagraph& ClickedPara =
                RenderedParagraphs[ClickedParaIndex];

            const float LocalLineY =
                ScriptY - ClickedPara.TopY;

            ClickedLineIndex =
                FMath::Clamp(
                    FMath::FloorToInt(LocalLineY / ScriptFormat::LineHeight),
                    0,
                    ClickedPara.Lines.Num() - 1
                );
        }


        // Where should this shot be inserted?
        const int32 InsertIndex = ResolveShotInsertIndex(SceneBlockId, ScriptY); // 0..NumShots

        // Shift existing shots down to make room (ShotIndex is 1-based)
        const int32 NewShotIndex = InsertIndex + 1;

        for (FGASMarker& M : SourceScript->Markers)
        {
            if (M.MarkerType == EGASMarkerType::ShotMarker &&
                M.BlockId == SceneBlockId &&
                M.ShotIndex >= NewShotIndex)
            {
                M.ShotIndex += 1;
            }
        }

        // Add the new marker at the inserted position
        FGASMarker NewMarker;
        NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        NewMarker.MarkerType = EGASMarkerType::ShotMarker;
        NewMarker.BlockId = SceneBlockId;
        NewMarker.ShotOrigin = EGASShotOrigin::User;
        NewMarker.ShotIndex = NewShotIndex;

        NewMarker.LineIndex = ClickedLineIndex;
        NewMarker.ParagraphIndex = ClickedParaIndex;

        NewMarker.ScreenX = LocalPos.X;
        NewMarker.ScreenY = ScriptY;




        SourceScript->Markers.Add(NewMarker);

        UE_LOG(LogTemp, Warning,
            TEXT("[ShotAdd] Inserted ShotIndex=%d Scene=%s"),
            NewShotIndex,
            *SceneBlockId
        );

        OnScriptMutated.ExecuteIfBound();

        // stays armed
        bNeedsRedraw = true;
        return FReply::Handled();
    }



    // ============================================================
    // SAFETY
    // ============================================================
    if (!SourceScript || RenderedParagraphs.Num() == 0)
    {
        return FReply::Unhandled();
    }

    // ============================================================
    // 3️⃣ PAGE BREAK DRAG (ALT + LMB)
    // ============================================================
    if (bEditMode && MouseEvent.IsAltDown() && bLeftMouse)
    {
        constexpr float PageBreakHitTolerance = 40.f;

        for (int32 BreakIndex = 0;
            BreakIndex < SourceScript->UserPageBreaks.Num();
            ++BreakIndex)
        {
            const FString& AfterBlockId =
                SourceScript->UserPageBreaks[BreakIndex].AfterBlockId;

            for (const FRenderedParagraph& P : RenderedParagraphs)
            {
                if (!P.bStartsPage)
                    continue;

                const int32 PrevIndex = P.ParagraphIndex - 1;
                if (!RenderedParagraphs.IsValidIndex(PrevIndex))
                    continue;

                if (RenderedParagraphs[PrevIndex].BlockId != AfterBlockId)
                    continue;

                if (FMath::Abs(MouseY - P.TopY) <= PageBreakHitTolerance)
                {
                    bIsDraggingPageBreak = true;
                    DraggedPageBreakIndex = BreakIndex;
                    DragPreviewY = P.TopY;

                    return FReply::Handled()
                        .CaptureMouse(AsShared());
                }
            }
        }

        return FReply::Handled();
    }

    // ============================================================
    // 4️⃣ ADD MODE (BLOCK INSERT)
    // ============================================================
    if (bAddMode && bLeftMouse)
    {
        const int32 InsertAfter = HitTestAddGutter(MouseY);
        if (InsertAfter != INDEX_NONE)
        {
            OpenAddBlockDialog(InsertAfter);
        }
        return FReply::Handled();
    }

    // ============================================================
    // 5️⃣ PARAGRAPH SELECTION + EDIT ROUTING
    // ============================================================
    int32 HitParagraphIndex = INDEX_NONE;

    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];
        if (MouseY >= P.TopY && MouseY <= (P.TopY + P.Height))
        {
            HitParagraphIndex = i;
            break;
        }
    }

    if (HitParagraphIndex == INDEX_NONE)
        return FReply::Handled();

    SelectedParagraphIndex = HitParagraphIndex;
    Invalidate(EInvalidateWidget::Paint);

    // Defer edit popup one tick so selection highlight paints first
    if (bEditMode && bLeftMouse && OnParagraphClicked.IsBound())
    {
        const int32 ParaToEdit = RenderedParagraphs[HitParagraphIndex].ParagraphIndex;

        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda(
                [this, ParaToEdit](float)
                {
                    if (OnParagraphClicked.IsBound())
                    {
                        OnParagraphClicked.Execute(ParaToEdit);
                    }
                    return false; // one-shot
                }
            )
        );

        return FReply::Handled();
    }



    return FReply::Handled();
}



FReply SGASScriptPanel::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    // ------------------------------------------------------------
    // Shot tail resize UPDATE
    // ------------------------------------------------------------
    if (bIsResizingShotTail)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        ShotTailGhostY = LocalPos.Y + ScrollOffsetY;

        bNeedsRedraw = true;
        return FReply::Handled();
    }


    // ------------------------------------------------------------
    // Shot Marker Drag UPDATE
    // ------------------------------------------------------------
    if (bIsDraggingShot)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        ShotDragGhostX = LocalPos.X - ShotDragOffset.X;
        ShotDragGhostY = (LocalPos.Y + ScrollOffsetY) - ShotDragOffset.Y;

        bNeedsRedraw = true;
        return FReply::Handled();
    }


    // ------------------------------------------------------------
    // Shot Range Drag UPDATE
    // ------------------------------------------------------------
    if (bIsShotRangeDragging)
    {
        const FVector2D DragLocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        const float ScriptY = DragLocalPos.Y + ScrollOffsetY;

        // 🔑 KEEP GHOST Y UPDATED FOR LIVE LINE DRAW
        ShotGhostY = ScriptY;

        const int32 ParaIndex = HitTestParagraph(ScriptY);

        if (ParaIndex != INDEX_NONE &&
            ParaIndex != ShotRangeCurrentParagraph)
        {
            ShotRangeCurrentParagraph = ParaIndex;
        }

        bNeedsRedraw = true;
        return FReply::Handled();
    }



    // ------------------------------------------------------------
    // Shot Add Hover Preview (works OVER pills + between pills)
    // ------------------------------------------------------------
    if (bShotAddArmed)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        const float ScriptY = LocalPos.Y + ScrollOffsetY;

        // Find owning SceneHeading for this Y (even if mouse is in shot stack area)
        const int32 SceneParaIndex = ResolveSceneIndexAtY(LocalPos.Y);

        int32 NewHoverIndex = INDEX_NONE;

        if (RenderedParagraphs.IsValidIndex(SceneParaIndex) &&
            RenderedParagraphs[SceneParaIndex].BlockType == EGASBlockType::SceneHeading)
        {
            NewHoverIndex = SceneParaIndex;
        }

        HoveredShotParagraphIndex = NewHoverIndex;

        // Cursor-follow Y in script space (NO snapping)
        ShotGhostY = ScriptY;
        bNeedsRedraw = true;


        return FReply::Unhandled();
    }



    
    // ------------------------------------------------------------
    // ADD MODE: no mouse-move behavior yet
    // ------------------------------------------------------------
    if (bAddMode)
    {
        return FReply::Unhandled();
    }

    // ------------------------------------------------------------
    // Safety guards
    // ------------------------------------------------------------
    if (!SourceScript || RenderedParagraphs.Num() == 0)
    {
        return FReply::Unhandled();
    }

    // ------------------------------------------------------------
    // Shot hover (selection preview)
    // ------------------------------------------------------------
    if (!bShotAddArmed)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        const float ScriptY = LocalPos.Y + ScrollOffsetY;


        const FString NewHoverShotId = HitTestShot(LocalPos);

        if (HoveredShotMarkerId != NewHoverShotId)
        {
            HoveredShotMarkerId = NewHoverShotId;
            bNeedsRedraw = true;
        }

    }

    if (bIsDraggingPageBreak && bEditMode)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        DragPreviewY = LocalPos.Y + ScrollOffsetY;

        Invalidate(EInvalidateWidget::Paint);

        return FReply::Handled();
    }



    // ------------------------------------------------------------
    // Existing hover logic (if any)
    // ------------------------------------------------------------
    return FReply::Unhandled();
}


FReply SGASScriptPanel::OnMouseButtonUp(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
    // Convert mouse position to local Y
    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const float ReleaseY = LocalPos.Y + ScrollOffsetY;

    // ------------------------------------------------------------
    // Shot tail resize COMMIT
    // ------------------------------------------------------------
    if (bIsResizingShotTail)
    {
        // UNDO SNAPSHOT — before mutation
        SourceScript->CaptureUndoSnapshot();

        for (FGASMarker& M : SourceScript->Markers)
        {
            if (M.Id == ResizingShotMarkerId)
            {
                M.ShotLineEndY = ShotTailGhostY;
                break;
            }
        }

        bIsResizingShotTail = false;
        ResizingShotMarkerId.Empty();
        ShotTailGhostY = -1.f;

        OnScriptMutated.ExecuteIfBound();
        bNeedsRedraw = true;

        return FReply::Handled().ReleaseMouseCapture();
    }




    // ------------------------------------------------------------
    // Shot Marker Drag COMMIT
    // ------------------------------------------------------------
    if (bIsDraggingShot)
    {
        // UNDO SNAPSHOT — before mutation
        SourceScript->CaptureUndoSnapshot();

        for (FGASMarker& M : SourceScript->Markers)
        {
            if (M.Id == DraggingShotMarkerId)
            {
                M.ScreenX = ShotDragGhostX;
                M.ScreenY = ShotDragGhostY;

                // update paragraph + line
                const int32 ParaIndex = HitTestParagraph(M.ScreenY);
                M.ParagraphIndex = ParaIndex;

                if (RenderedParagraphs.IsValidIndex(ParaIndex))
                {
                    const FRenderedParagraph& P = RenderedParagraphs[ParaIndex];
                    M.LineIndex = FMath::Clamp(
                        FMath::FloorToInt((M.ScreenY - P.TopY) / ScriptFormat::LineHeight),
                        0,
                        FMath::Max(0, P.Lines.Num() - 1)
                    );
                }
                break;
            }
        }

        bIsDraggingShot = false;
        DraggingShotMarkerId.Empty();
        ShotDragGhostX = ShotDragGhostY = -1.f;

        OnScriptMutated.ExecuteIfBound();
        bNeedsRedraw = true;

        return FReply::Handled().ReleaseMouseCapture();
    }



    // ------------------------------------------------------------
    // Shot Range Drag END → COMMIT SINGLE SHOT (midpoint-based)
    // ------------------------------------------------------------
    if (bIsShotRangeDragging)
    {
        bIsShotRangeDragging = false;

        const int32 Start =
            FMath::Min(ShotRangeStartParagraph, ShotRangeCurrentParagraph);

        const int32 End =
            FMath::Max(ShotRangeStartParagraph, ShotRangeCurrentParagraph);

        // Find owning scene
        const int32 SceneParaIndex =
            FindOwningSceneParagraphIndex(Start);

        if (SceneParaIndex != INDEX_NONE &&
            RenderedParagraphs.IsValidIndex(SceneParaIndex))
        {
            const FRenderedParagraph& ScenePara =
                RenderedParagraphs[SceneParaIndex];

            if (ScenePara.BlockType == EGASBlockType::SceneHeading)
            {
                // Midpoint paragraph → script Y
                const float MidY =
                    (RenderedParagraphs[Start].TopY +
                        RenderedParagraphs[End].TopY +
                        RenderedParagraphs[End].Height) * 0.5f;

                // Derive insert index from vertical intent
                const float ShotStartY =
                    ScenePara.TopY + ScenePara.Height + 4.f;

                const int32 InsertIndex =
                    (MidY <= ShotStartY)
                    ? 0
                    : ResolveShotInsertIndex(ScenePara.BlockId, MidY);

                const int32 StartPara = ShotRangeStartParagraph;
                const FRenderedParagraph& StartP = RenderedParagraphs[StartPara];

                // Script-space Y at start of drag
                const float StartScriptY =
                    StartP.TopY + (0.5f * ScriptFormat::LineHeight);

                // PATCH CALL SITE — remove extra parameter

                CommitShotAtParagraph(
                    SceneParaIndex,
                    InsertIndex,
                    ShotRangeStartY,
                    ReleaseY,
                    ShotRangeStartX
                );






                OnScriptMutated.ExecuteIfBound();
            }
        }

        ShotRangeStartParagraph = INDEX_NONE;
        ShotRangeCurrentParagraph = INDEX_NONE;

        bNeedsRedraw = true;
        return FReply::Handled().ReleaseMouseCapture();
    }




    // ------------------------------------------------------------
    // SHOT ADD COMMIT (takes priority over page breaks)
    // ------------------------------------------------------------
    //if (bShotAddArmed && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    //{
    //    const float LocalY =
    //        MyGeometry.AbsoluteToLocal(
    //            MouseEvent.GetScreenSpacePosition()).Y;

    //    const int32 SceneParaIndex = ResolveSceneIndexAtY(LocalY);

    //    if (SceneParaIndex != INDEX_NONE && SourceScript)
    //    {
    //        const FString& SceneBlockId =
    //            RenderedParagraphs[SceneParaIndex].BlockId;

    //        FGASMarker NewMarker;
    //        NewMarker.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    //        NewMarker.MarkerType = EGASMarkerType::ShotMarker;
    //        NewMarker.BlockId = SceneBlockId;
    //        NewMarker.ShotOrigin = EGASShotOrigin::User;

    //        SourceScript->Markers.Add(NewMarker);

    //        UE_LOG(LogTemp, Warning,
    //            TEXT("[SHOT ADD] Scene=%s"),
    //            *SceneBlockId);

    //        OnScriptMutated.ExecuteIfBound();
    //    }

    //    return FReply::Handled();
    //}


    // Stop dragging
    bIsDraggingPageBreak = false;




    // ------------------------------------------------------------
    // Snap to nearest paragraph
    // ------------------------------------------------------------
    int32 NewAfterParagraph = INDEX_NONE;

    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        // We snap to paragraphs that participate in vertical flow
        if (P.ParagraphIndex != INDEX_NONE)
        {
            if (ReleaseY < P.TopY)
            {
                NewAfterParagraph = P.ParagraphIndex - 1;
                break;
            }
        }

    }

    // Fallback: place at end if dropped below everything
    if (NewAfterParagraph == INDEX_NONE && RenderedParagraphs.Num() > 0)
    {
        NewAfterParagraph =
            RenderedParagraphs.Last().ParagraphIndex;
    }

    // ------------------------------------------------------------
    // Commit page break change
    // ------------------------------------------------------------
    if (SourceScript &&
        SourceScript->UserPageBreaks.IsValidIndex(DraggedPageBreakIndex) &&
        RenderedParagraphs.IsValidIndex(NewAfterParagraph))
    {
        const FString& NewAfterBlockId =
            RenderedParagraphs[NewAfterParagraph].BlockId;

        SourceScript->UserPageBreaks[DraggedPageBreakIndex]
            .AfterBlockId = NewAfterBlockId;
    }


    // Prepare snap animation
    SnapStartY = DragPreviewY;

    // Find target Y from the newly snapped paragraph
    SnapTargetY = DragPreviewY;
    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        if (P.ParagraphIndex == NewAfterParagraph + 1 && P.bStartsPage)
        {
            SnapTargetY = P.TopY - (ScriptFormat::LineHeight * 2.0f);
            break;
        }
    }

    bAnimatingPageBreak = true;
    SnapAnimAlpha = 0.f;

    DraggedPageBreakIndex = INDEX_NONE;

    // Rebuild layout so page numbers update
    RebuildLayout();

    // Release mouse capture
    return FReply::Handled().ReleaseMouseCapture();

}

FReply SGASScriptPanel::OnMouseWheel(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent)
{
    return FReply::Unhandled();
}


FCursorReply SGASScriptPanel::OnCursorQuery(
    const FGeometry& MyGeometry,
    const FPointerEvent& CursorEvent
) const
{
    // Page break hover takes priority
    if (HoveredPageBreakIndex != INDEX_NONE)
    {
        return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
    }

    // Shot add mode cursor logic
    if (bShotAddArmed && RenderedParagraphs.Num() > 0)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());

        const float ScriptY = LocalPos.Y + ScrollOffsetY;

        const int32 SceneParaIndex = ResolveSceneIndexAtY(LocalPos.Y);

        if (RenderedParagraphs.IsValidIndex(SceneParaIndex))
        {
            const FRenderedParagraph& ScenePara =
                RenderedParagraphs[SceneParaIndex];

            // 🚫 Dead zone directly under Scene Heading → normal cursor
            if (ScriptY <= ScenePara.TopY + ScenePara.Height + ScriptFormat::LineHeight)
            {
                return FCursorReply::Cursor(EMouseCursor::Default);
            }

            // ✅ Valid shot area → camera cursor
            return FCursorReply::Cursor(EMouseCursor::Crosshairs);
        }
    }

    return FCursorReply::Unhandled();
}



void SGASScriptPanel::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
{
    // Recompute total height before Slate asks for desired size
    CachedTotalHeight = 0.f;

    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        CachedTotalHeight = FMath::Max(
            CachedTotalHeight,
            P.TopY + P.Height
        );
    }

    Invalidate(EInvalidateWidget::Paint);

    // ------------------------------------------------------------
    // Page break snap animation
    // ------------------------------------------------------------
    if (bAnimatingPageBreak)
    {
        constexpr float SnapSpeed = 8.0f; // higher = snappier

        SnapAnimAlpha = FMath::Min(1.f, SnapAnimAlpha + InDeltaTime * SnapSpeed);

        // Ease-out (feels better than linear)
        const float T = 1.f - FMath::Pow(1.f - SnapAnimAlpha, 3);

        DragPreviewY = FMath::Lerp(SnapStartY, SnapTargetY, T);

        if (SnapAnimAlpha >= 1.f)
        {
            bAnimatingPageBreak = false;
        }

        Invalidate(EInvalidateWidget::Paint);

        if (PendingEditBlockIndex != INDEX_NONE)
        {
            const int32 ParagraphIndexToEdit = PendingEditBlockIndex;
            PendingEditBlockIndex = INDEX_NONE;

            // Exit Add Mode before opening editor
            const bool bWasAddMode = bAddMode;
            bAddMode = false;

            TryEditParagraph(ParagraphIndexToEdit);

            // (Optional) restore if you want
            // bAddMode = bWasAddMode;
        }



    }

}

// =========================================================
// Shot Selection
// =========================================================

void SGASScriptPanel::EnterShotSelectionMode()
{
    bIsShotSelectMode = true;
    bIsDraggingShot = false;

    ShotStartBlockId.Empty();
    ShotEndBlockId.Empty();

    ShotStartParagraphIndex = INDEX_NONE;
    ShotCurrentParagraphIndex = INDEX_NONE;

    UE_LOG(LogTemp, Warning, TEXT("[SHOT] Shot selection mode ARMED"));
}


int32 SGASScriptPanel::ResolveSceneIndexAtY(float LocalY) const
{
    for (int32 i = RenderedParagraphs.Num() - 1; i >= 0; --i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        if (P.BlockType == EGASBlockType::SceneHeading &&
            LocalY >= P.TopY - ScrollOffsetY)
        {
            return i;
        }
    }
    return INDEX_NONE;
}

int32 SGASScriptPanel::CountShotsForScene(const FString& SceneBlockId) const
{
    int32 Count = 0;

    if (!SourceScript)
        return 0;

    for (const FGASMarker& M : SourceScript->Markers)
    {
        if (M.MarkerType == EGASMarkerType::ShotMarker &&
            M.BlockId == SceneBlockId)
        {
            ++Count;
        }
    }

    return Count;
}


int32 SGASScriptPanel::ResolveShotInsertIndex(
    const FString& SceneBlockId,
    float ScriptY
) const
{
    if (!SourceScript)
        return 0;

    // Gather shots for this scene
    TArray<const FGASMarker*> SceneShots;
    for (const FGASMarker& M : SourceScript->Markers)
    {
        if (M.MarkerType == EGASMarkerType::ShotMarker &&
            M.BlockId == SceneBlockId)
        {
            SceneShots.Add(&M);
        }
    }

    // Sort by current order
    SceneShots.Sort(
        [](const FGASMarker& A, const FGASMarker& B)
        {
            return A.ShotIndex < B.ShotIndex;
        }
    );

    // Find owning scene paragraph
    int32 SceneParaIndex = INDEX_NONE;
    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        if (RenderedParagraphs[i].BlockId == SceneBlockId)
        {
            SceneParaIndex = i;
            break;
        }
    }

    if (SceneParaIndex == INDEX_NONE)
        return SceneShots.Num();

    const FRenderedParagraph& ScenePara =
        RenderedParagraphs[SceneParaIndex];

    const float ShotStartY =
        ScenePara.TopY + ScenePara.Height + 4.f;

    constexpr float ShotRowHeight = 20.f;

    // Convert Y to relative shot space
    const float LocalY =
        ScriptY - ShotStartY;

    if (LocalY <= 0.f)
        return 0;

    const int32 Index =
        FMath::FloorToInt(LocalY / ShotRowHeight);

    return FMath::Clamp(Index, 0, SceneShots.Num());
}


void SGASScriptPanel::SetAddShotArmed(bool bArmed)
{
    bShotAddArmed = bArmed;
    HoveredShotParagraphIndex = INDEX_NONE;
    bNeedsRedraw = true;
}

void SGASScriptPanel::SetShotAddArmed(bool bInArmed)
{
    bShotAddArmed = bInArmed;

    if (!bShotAddArmed)
    {
        HoveredShotParagraphIndex = INDEX_NONE;
        HoveredShotMarkerId.Empty();
        SelectedShotMarkerId.Empty();
    }

    bNeedsRedraw = true;
    Invalidate(EInvalidateWidget::Paint);
}

int32 SGASScriptPanel::FindOwningSceneParagraphIndex(int32 StartParagraphIndex) const
{
    if (!RenderedParagraphs.IsValidIndex(StartParagraphIndex))
        return INDEX_NONE;

    for (int32 i = StartParagraphIndex; i >= 0; --i)
    {
        if (RenderedParagraphs[i].BlockType == EGASBlockType::SceneHeading)
        {
            return i;
        }
    }

    return INDEX_NONE;
}


FString SGASScriptPanel::HitTestShot(const FVector2D& LocalPos) const
{
    constexpr float HitPadding = 6.f;

    for (const FShotHitRect& HR : ShotHitRects)
    {
        FSlateRect Expanded =
            FSlateRect(
                HR.Rect.Left - HitPadding,
                HR.Rect.Top - HitPadding,
                HR.Rect.Right + HitPadding,
                HR.Rect.Bottom + HitPadding
            );

        if (Expanded.ContainsPoint(LocalPos))
        {
            return HR.MarkerId;
        }
    }

    return FString();
}

FString SGASScriptPanel::HitTestShotTail(const FVector2D& LocalPos) const
{
    constexpr float TailHitHeight = 6.f;
    constexpr float TailHitWidth = 12.f;

    for (const FGASMarker& M : SourceScript->Markers)
    {
        if (M.MarkerType != EGASMarkerType::ShotMarker)
            continue;

        if (M.ShotLineEndY < 0.f)
            continue;

        const float TailX = M.ScreenX + (ShotPillWidth * 0.5f);
        const float TailY = M.ShotLineEndY - ScrollOffsetY;

        FSlateRect TailRect(
            TailX - TailHitWidth,
            TailY - TailHitHeight,
            TailX + TailHitWidth,
            TailY + TailHitHeight
        );

        if (TailRect.ContainsPoint(LocalPos))
        {
            return M.Id;
        }
    }

    return FString();
}

