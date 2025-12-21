// NOTE:
// This panel NEVER mutates script content.
// All edits go through the script edit pipeline (JSON → ReLayout).


#include "SGASScriptPanel.h"
#include "ScriptFormatRules.h"
#include "SlateOptMacros.h"
#include "GAS_PreProToolsStyle.h"
#include "Modules/ModuleManager.h"
#include "GAS_PreProToolsEditorModule.h"

#include "ScriptLayoutEngine.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

#include "SlateBasics.h"
#include "SlateExtras.h"
#include "Containers/Ticker.h"



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
            SourceScript->UserPageBreaks
        );


    SetRenderedScript(NewRendered);   // updates RenderedParagraphs + bNeedsRedraw

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
FVector2D SGASScriptPanel::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
    return FVector2D(1000.f, CachedTotalHeight + 200.f);
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

    auto HasPageBreakAfterParagraph = [&](int32 ParagraphIndex) -> bool
        {
            for (const FGASUserPageBreak& PB : SourceScript->UserPageBreaks)
            {
                if (PB.AfterParagraphIndex == ParagraphIndex)
                    return true;
            }
            return false;
        };



    switch (Edit.Type)
    {
    case EGASScriptEditType::SetPageBreak:
    {
        const int32 ParagraphIndex = Edit.ParagraphIndex;

        if (ParagraphIndex != INDEX_NONE)
        {
            if (!HasPageBreakAfterParagraph(ParagraphIndex))
            {
                FGASUserPageBreak NewBreak;
                NewBreak.AfterParagraphIndex = ParagraphIndex;

                SourceScript->UserPageBreaks.Add(NewBreak);
            }
        }
        break;
    }



    case EGASScriptEditType::ClearPageBreak:
    {
        const int32 ParagraphIndex = Edit.ParagraphIndex;

        if (ParagraphIndex != INDEX_NONE)
        {
            SourceScript->UserPageBreaks.RemoveAll(
                [&](const FGASUserPageBreak& PB)
                {
                    return PB.AfterParagraphIndex == ParagraphIndex;
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


// =============================================================
//  Edit Dialog
// =============================================================

//void SGASScriptPanel::OpenEditDialog(int32 ParagraphIndex)
//{
//    if (!RenderedParagraphs.IsValidIndex(ParagraphIndex))
//        return;
//
//    const FRenderedParagraph& P = RenderedParagraphs[ParagraphIndex];
//
//    // ------------------------------------------------------------
//    // Capture original text
//    // ------------------------------------------------------------
//    FString OriginalText = GetSourceTextForBlock(P.BlockId);
//
//    // We store edited text here before commit
//    TSharedRef<FString> EditedText =
//        MakeShared<FString>(OriginalText);
//
//    // ------------------------------------------------------------
//    // Build modal window
//    // ------------------------------------------------------------
//    TSharedRef<SWindow> EditWindow = SNew(SWindow)
//        .Title(FText::FromString(TEXT("Edit Paragraph")))
//        .ClientSize(FVector2D(600.f, 400.f))
//        .SupportsMinimize(false)
//        .SupportsMaximize(false);
//
//    EditWindow->SetContent(
//        SNew(SVerticalBox)
//
//        // -----------------------------
//        // Text editor
//        // -----------------------------
//        +SVerticalBox::Slot()
//        .FillHeight(1.f)
//        .Padding(8.f)
//        [
//            SNew(SMultiLineEditableTextBox)
//                .Text(FText::FromString(OriginalText))
//                .OnTextChanged_Lambda(
//                    [EditedText](const FText& NewText)
//                    {
//                        *EditedText = NewText.ToString();
//                    })
//        ]
//
//    // -----------------------------
//    // Buttons
//    // -----------------------------
//    +SVerticalBox::Slot()
//        .AutoHeight()
//        .Padding(8.f)
//        [
//            SNew(SHorizontalBox)
//
//                + SHorizontalBox::Slot()
//                .HAlign(HAlign_Right)
//                .FillWidth(1.f)
//                [
//                    SNew(SButton)
//                        .Text(FText::FromString(TEXT("Cancel")))
//                        .OnClicked_Lambda(
//                            [EditWindow]()
//                            {
//                                EditWindow->RequestDestroyWindow();
//                                return FReply::Handled();
//                            })
//                ]
//
//            + SHorizontalBox::Slot()
//                .HAlign(HAlign_Right)
//                .AutoWidth()
//                .Padding(8.f, 0.f)
//                [
//                    SNew(SButton)
//                        .Text(FText::FromString(TEXT("OK")))
//                        .OnClicked_Lambda(
//                            [this, EditWindow, P, EditedText]()
//                            {
//                                ApplyTextEdit(P.BlockId, *EditedText);
//
//                                // If this was Add Character+Dialogue, open Dialogue next
//                                if (PendingDialogueAfterCharacterIndex != INDEX_NONE)
//                                {
//                                    const int32 DialogueIndex = PendingDialogueAfterCharacterIndex;
//                                    PendingDialogueAfterCharacterIndex = INDEX_NONE;
//
//                                    FTSTicker::GetCoreTicker().AddTicker(
//                                        FTickerDelegate::CreateLambda(
//                                            [this, DialogueIndex](float)
//                                            {
//                                                OpenEditDialogueDialog(DialogueIndex);
//                                                return false;
//                                            }
//                                        )
//                                    );
//
//
//
//                                }
//
//                                EditWindow->RequestDestroyWindow();
//                                return FReply::Handled();
//                            })
//
//                ]
//        ]
//        );
//
//    // ------------------------------------------------------------
//    // Show modal
//    // ------------------------------------------------------------
//    FSlateApplication::Get().AddWindow(EditWindow);
//}


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
    UE_LOG(LogTemp, Error, TEXT("TRY EDIT CALLED: BlockIndex=%d"), BlockIndex);

    if (!SourceScript)
    {
        UE_LOG(LogTemp, Error, TEXT("SCRIPT ASSET IS NULL"));
        return;
    }

    if (!SourceScript->Blocks.IsValidIndex(BlockIndex))
    {
        UE_LOG(LogTemp, Error, TEXT(
            "INVALID BLOCK INDEX %d (Blocks.Num=%d)"),
            BlockIndex,
            SourceScript->Blocks.Num()
        );
        return;
    }

    FGASBlock& Block = SourceScript->Blocks[BlockIndex];

    UE_LOG(LogTemp, Error, TEXT("BLOCK TYPE = %d"), (int32)Block.Type);

    switch (Block.Type)
    {
    case EGASBlockType::Action:
        UE_LOG(LogTemp, Error, TEXT("OPEN ACTION EDIT"));
        OpenEditActionDialog(BlockIndex);
        break;

    case EGASBlockType::Character:
        UE_LOG(LogTemp, Error, TEXT("OPEN CHARACTER EDIT"));
        OpenEditCharacterDialog(BlockIndex);
        break;

    case EGASBlockType::Dialogue:
        UE_LOG(LogTemp, Error, TEXT("OPEN DIALOGUE EDIT"));
        OpenEditDialogueDialog(BlockIndex);
        break;

    default:
        UE_LOG(LogTemp, Error, TEXT("UNSUPPORTED BLOCK TYPE"));
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
                                            SourceScript->Blocks[BlockIndex].Text = CachedEditText.ToString();

                                            // mark dirty so the UI shows * and user knows to save JSON
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

    // 0 = Action, 1 = Character + Dialogue
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

    // Edit and Add are mutually exclusive
    if (bEditMode)
    {
        bAddMode = false;
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

void SGASScriptPanel::InsertNewBlock(
    int32 InsertAfterParagraphIndex,
    int32 BlockTypeChoice
)
{
    if (!SourceScript)
    {
        return;
    }

    const int32 InsertIndex = InsertAfterParagraphIndex + 1;

    if (BlockTypeChoice == 0)
    {
        FGASBlock NewBlock;
        NewBlock.Type = EGASBlockType::Action;
        NewBlock.Text = TEXT("");

        SourceScript->Blocks.Insert(NewBlock, InsertIndex);

        // Track add for cancel rollback
        PendingAddStartIndex = InsertIndex;
        PendingAddBlockCount = 1;

        PendingEditBlockIndex = InsertIndex;

    }
    else
    {
        FGASBlock CharacterBlock;
        CharacterBlock.Type = EGASBlockType::Character;
        CharacterBlock.Text = TEXT("");

        FGASBlock DialogueBlock;
        DialogueBlock.Type = EGASBlockType::Dialogue;
        DialogueBlock.Text = TEXT("");

        SourceScript->Blocks.Insert(CharacterBlock, InsertIndex);
        SourceScript->Blocks.Insert(DialogueBlock, InsertIndex + 1);

        // Track add for cancel rollback
        PendingAddStartIndex = InsertIndex;
        PendingAddBlockCount = 2;

        // Edit CHARACTER first
        PendingEditBlockIndex = InsertIndex;

        // Remember dialogue for next step
        PendingDialogueAfterCharacterIndex = InsertIndex + 1;


    }

    // Mark dirty
    FGAS_PreProToolsEditorModule& Module =
        FModuleManager::LoadModuleChecked<FGAS_PreProToolsEditorModule>(
            "GAS_PreProToolsEditor"
        );

    Module.MarkToolDirty();


    RebuildLayout();
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

    // If this edit came from Add Mode, roll it back
    if (PendingAddBlockCount > 0 &&
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

    const FSlateFontInfo FontInfo =
        FGAS_PreProToolsStyle::Get().GetFontStyle("GAS.ScriptFont");

    const TSharedRef<FSlateFontMeasure> FontMeasureService =
        FSlateApplication::Get().GetRenderer()->GetFontMeasureService();



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
                    FVector2D(0.f, P.TopY),
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
            const FString* SceneNum =
                P.SourceMetadata.Find("SceneNumber");

            if (SceneNum)
            {
                FSlateDrawElement::MakeText(
                    OutDrawElements,
                    LayerId,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(ScriptFormat::PageLeft + P.IndentLeft - 50.f, P.TopY),
                        FVector2D::UnitVector
                    ),
                    FText::FromString(*SceneNum),
                    FontInfo,
                    ESlateDrawEffect::None,
                    FLinearColor(0.65f, 0.65f, 0.65f, 1.f)
                );
            }
        }

        // ------------------------------------------------------------
        // Draw PAGE BREAK marker (derived, visual only)
        // ------------------------------------------------------------
        if (P.bStartsPage)
        {
            const FString PageText =
                FString::Printf(TEXT("PAGE %d"), P.PageNumber);

            const float MarkerY =
                P.TopY - (ScriptFormat::LineHeight * 2.0f);

            const float MarkerX =
                ScriptFormat::PageLeft * 0.35f;

            const bool bIsHovered =
                bEditMode &&
                (HoveredPageBreakIndex != INDEX_NONE) &&
                SourceScript &&
                SourceScript->UserPageBreaks.IsValidIndex(HoveredPageBreakIndex) &&
                SourceScript->UserPageBreaks[HoveredPageBreakIndex].AfterParagraphIndex
                == P.ParagraphIndex - 1;


            const FLinearColor TextColor =
                bIsHovered
                ? FLinearColor(1.0f, 1.0f, 0.6f, 1.f)   // bright yellow
                : FLinearColor(1.0f, 0.85f, 0.2f, 1.f); // normal yellow

            // Bold horizontal rule when hovered
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

            // Page text
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
        float LineY = P.TopY;

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
                FLinearColor::White
            );

            LineY += ScriptFormat::LineHeight;
        }

        // ----------------------------------------------
        // Update panel height
        // ----------------------------------------------
        CachedTotalHeight = FMath::Max(CachedTotalHeight, P.TopY + P.Height);
    }

    // ------------------------------------------------------------
    // Draw page break drag preview
    // ------------------------------------------------------------
    if (bIsDraggingPageBreak || bAnimatingPageBreak)
    {
        const float LineThickness = 2.0f;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId + 50,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(0.f, DragPreviewY),
                FVector2D(AllottedGeometry.GetLocalSize().X, LineThickness)
            ),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            FLinearColor(1.0f, 0.85f, 0.2f, 0.9f)
        );
    }



    bNeedsRedraw = false;
    return LayerId + 10;
}



int32 SGASScriptPanel::HitTestParagraph(float LocalY) const
{
    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        if (LocalY >= P.TopY && LocalY <= (P.TopY + P.Height))
        {
            return i;
        }
    }

    return INDEX_NONE;
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


void SGASScriptPanel::ScrollToParagraph(int32 ParagraphIndex)
{
    // Future: implement scrolling. For now, safe no-op.
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
    // SAFETY: panel not ready yet
    if (!SourceScript || RenderedParagraphs.Num() == 0)
    {
        return FReply::Unhandled();
    }

    UE_LOG(LogTemp, Warning, TEXT("SCRIPT PANEL: MouseDown received"));

    // We only handle clicks when a tool is active
    UE_LOG(LogTemp, Error, TEXT("PANEL STATE: Edit=%d Add=%d"), bEditMode, bAddMode);



    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    if (RenderedParagraphs.Num() == 0)
    {
        return FReply::Unhandled();
    }



    const bool bAltDown = MouseEvent.IsAltDown();
    const bool bCtrlDown = MouseEvent.IsControlDown();


    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const float MouseY = LocalPos.Y;

    // ------------------------------------------------------------
    // ADD MODE: click between blocks to insert
    // ------------------------------------------------------------
    if (bAddMode)
    {
        const int32 InsertAfterParagraph =
            HitTestAddGutter(MouseY);

        if (InsertAfterParagraph != INDEX_NONE)
        {
            OpenAddBlockDialog(InsertAfterParagraph);
            return FReply::Handled();
        }

        return FReply::Handled();
    }


    // ------------------------------------------------------------
    // 1) PAGE BREAK HIT TEST (ALT + LMB ONLY)
    // ------------------------------------------------------------
    if (bAltDown)
    {
        constexpr float PageBreakHitTolerance = 40.f;

        for (int32 BreakIndex = 0; BreakIndex < SourceScript->UserPageBreaks.Num(); ++BreakIndex)
        {
            const int32 AfterParagraph =
                SourceScript->UserPageBreaks[BreakIndex].AfterParagraphIndex;


            for (const FRenderedParagraph& P : RenderedParagraphs)
            {
                if (P.ParagraphIndex == AfterParagraph + 1 && P.bStartsPage)
                {
                    const float MarkerY =
                        P.TopY - (ScriptFormat::LineHeight * 2.0f);

                    if (FMath::Abs(MouseY - MarkerY) <= PageBreakHitTolerance)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("PAGE BREAK HIT! Index=%d"), BreakIndex);

                        bIsDraggingPageBreak = true;
                        DraggedPageBreakIndex = BreakIndex;
                        DragPreviewY = MarkerY;

                        return FReply::Handled().CaptureMouse(AsShared());
                    }
                }
            }
        }
    }


    // ------------------------------------------------------------
    // 2) BLOCK HIT TEST (NEW)
    // ------------------------------------------------------------
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
    {
        UE_LOG(LogTemp, Error, TEXT("NO PARAGRAPH HIT — forcing paragraph 0"));
        HitParagraphIndex = 0;
    }


    // ------------------------------------------------------------
    // 3) SELECT PARAGRAPH (VISUAL FEEDBACK)
    // ------------------------------------------------------------
    SelectedParagraphIndex = HitParagraphIndex;
    Invalidate(EInvalidateWidget::Paint);

    const FRenderedParagraph& HitParagraph = RenderedParagraphs[HitParagraphIndex];


    // ------------------------------------------------------------
    // CTRL + LMB = Delete block (Edit Mode only)
    // ------------------------------------------------------------
    if (bEditMode && bCtrlDown)
    {
        OpenDeleteBlockDialog(HitParagraph.ParagraphIndex);
        return FReply::Handled();
    }


    // ------------------------------------------------------------
    // 4) ROUTE ALL BLOCK CLICKS THROUGH EDIT PIPELINE
    // ------------------------------------------------------------
    if (bEditMode && OnParagraphClicked.IsBound())
    {
        UE_LOG(LogTemp, Error, TEXT("EXECUTING OnParagraphClicked for %d"), HitParagraph.ParagraphIndex);
        OnParagraphClicked.Execute(HitParagraph.ParagraphIndex);
    }

    else
    {
        UE_LOG(LogTemp, Error, TEXT("OnParagraphClicked NOT bound"));
    }


    return FReply::Handled();

}



FReply SGASScriptPanel::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent
)
{
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
    // PAGE BREAK DRAGGING (Edit Mode only)
    // ------------------------------------------------------------
    if (bEditMode && bIsDraggingPageBreak)
    {
        const FVector2D LocalPos =
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

        DragPreviewY = LocalPos.Y;
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
    if (!bIsDraggingPageBreak)
    {
        return FReply::Unhandled();
    }

    // Stop dragging
    bIsDraggingPageBreak = false;

    // Convert mouse position to local Y
    const FVector2D LocalPos =
        MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    const float ReleaseY = LocalPos.Y;

    // ------------------------------------------------------------
    // Snap to nearest paragraph
    // ------------------------------------------------------------
    int32 NewAfterParagraph = INDEX_NONE;

    for (int32 i = 0; i < RenderedParagraphs.Num(); ++i)
    {
        const FRenderedParagraph& P = RenderedParagraphs[i];

        // We snap to paragraphs that participate in vertical flow
        if (!P.bStartsPage && P.ParagraphIndex != INDEX_NONE)
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
        SourceScript ->UserPageBreaks.IsValidIndex(DraggedPageBreakIndex))
    {
        SourceScript ->UserPageBreaks[DraggedPageBreakIndex]
            .AfterParagraphIndex = FMath::Max(0, NewAfterParagraph);
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

FCursorReply SGASScriptPanel::OnCursorQuery(
    const FGeometry& MyGeometry,
    const FPointerEvent& CursorEvent
) const
{
    if (HoveredPageBreakIndex != INDEX_NONE)
    {
        return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
    }

    return FCursorReply::Unhandled();
}


void SGASScriptPanel::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
{
    // Recompute total height before Slate asks for desired size
    float Y = 0.f;

    for (const FRenderedParagraph& P : RenderedParagraphs)
    {
        Y += P.Height;
    }

    CachedTotalHeight = Y;

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

