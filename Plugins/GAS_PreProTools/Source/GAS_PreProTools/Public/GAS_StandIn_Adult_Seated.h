#pragma once

#include "CoreMinimal.h"
#include "GAS_StandInActor.h"
#include "GAS_StandIn_Adult_Seated.generated.h"

UCLASS()
class GAS_PREPROTOOLS_API AGAS_StandIn_Adult_Seated : public AGAS_StandInActor
{
	GENERATED_BODY()

public:
	AGAS_StandIn_Adult_Seated();

	virtual EGASStandInType GetStandInType() const override;

protected:
	virtual FGASStandInPhysicalSpec GetPhysicalSpec() const;

};
