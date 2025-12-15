#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

enum class EGASMainTab : uint8
{
    Script,
    ShotList,
    Shots,
    Edit
};

class UGASPreProProject;


class SGAS_TestWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGAS_TestWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    void SetActiveTab(EGASMainTab NewTab);

    TSharedRef<SWidget> CreateScriptTabContent();
    TSharedRef<SWidget> CreateShotListTabContent();
    TSharedRef<SWidget> CreateShotsTabContent();
    TSharedRef<SWidget> CreateEditTabContent();

    EGASMainTab ActiveTab = EGASMainTab::Script;

    // Where the active tab's content is placed
    TSharedPtr<class SBox> ContentBox;

    FText GetScriptTabLabel() const;

    UGASPreProProject* ActiveProject = nullptr;
};

