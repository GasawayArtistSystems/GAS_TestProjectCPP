#pragma once

#include "CoreMinimal.h"
#include "GAS_StandInActor.h"
#include "GAS_StandIn_Adult_Average.generated.h"

UCLASS()
class GAS_PREPROTOOLS_API AGAS_StandIn_Adult_Average : public AGAS_StandInActor
{
	GENERATED_BODY()

public:
	AGAS_StandIn_Adult_Average();

	virtual EGASStandInType GetStandInType() const override;

protected:
	virtual FGASStandInPhysicalSpec GetPhysicalSpec() const;

};
