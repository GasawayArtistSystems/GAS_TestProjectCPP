#pragma once

#include "CoreMinimal.h"
#include "GAS_SetDescriptor.h"

class GAS_PREPROTOOLS_API FGASSetDiscovery
{
public:
	// Call once to populate cache
	static void BuildCache();

	// Read-only access to cached sets
	static const TArray<FGASSetDescriptor>& GetAvailableSets();

private:
	static void DiscoverSets(TArray<FGASSetDescriptor>& OutSets);

private:
	static TArray<FGASSetDescriptor> CachedSets;
	static bool bCacheBuilt;
};
