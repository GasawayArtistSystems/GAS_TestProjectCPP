#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "GAS_ShotListTypes.h"
#include "SGAS_ScriptTab.h"
#include "Delegates/Delegate.h"



enum class EGASMainTab : uint8
{
    Script,
    ShotList,
    Shots,
    Edit
};

DECLARE_DELEGATE_OneParam(FOnSceneSelected, int32 /*SceneBlockIndex*/);


class UGASPreProProject;
struct FGASScript;


class SGAS_TestWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGAS_TestWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void SetScript(const FGASScript& InScript)
    {
        Script = &InScript;
    }

    FOnSceneSelected OnSceneSelected;

private:
    void SetActiveTab(EGASMainTab NewTab);

    TSharedRef<SWidget> CreateScriptTabContent();
    TSharedRef<SWidget> CreateShotListTabContent();
    TSharedRef<SWidget> CreateShotsTabContent();
    TSharedRef<SWidget> CreateEditTabContent();

    EGASMainTab ActiveTab = EGASMainTab::Script;

    TWeakPtr<SGAS_ScriptTab> WeakScriptTab;


    // Where the active tab's content is placed
    TSharedPtr<class SBox> ContentBox;

    FText GetScriptTabLabel() const;

    UGASPreProProject* ActiveProject = nullptr;

    const FGASScript* Script = nullptr;

    TArray<TSharedPtr<FGASShotDefinitionListRow>> ShotListItems;
    TSharedRef<ITableRow> GenerateShotListRow(
        TSharedPtr<FGASShotDefinitionListRow> InItem,
        const TSharedRef<STableViewBase>& OwnerTable
    );

    TSharedPtr<SGAS_ScriptTab> ScriptTab;
    TSharedPtr<SGAS_ScriptTab> ScriptTabWidget;

    TSharedPtr<SListView<TSharedPtr<FGASShotDefinitionListRow>>> ShotListView;

};

