#pragma once

#include "CoreMinimal.h"

class FGASSetManager
{
public:
	static void SetActiveSet(FName InSetId);
	static FName GetActiveSet();

private:
	static FName ActiveSetId;
};
