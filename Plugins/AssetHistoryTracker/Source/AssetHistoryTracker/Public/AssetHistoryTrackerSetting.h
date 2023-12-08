// Copyright 2023 X-Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetHistoryTrackerSetting.generated.h"


USTRUCT()
struct FAssetHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FString AssetName;

	UPROPERTY()
	FString AssetPathName;

	UPROPERTY()
	UClass* Class;

	FAssetHistoryEntry()
	{
	}

	FAssetHistoryEntry(const FString& Name, const FString& PathName, UClass* Class)
		: AssetName(Name),
		  AssetPathName(PathName),
		  Class(Class)
	{
	}
};

UCLASS(config=EditorPerProjectUserSettings)
class ASSETHISTORYTRACKER_API UAssetHistoryTrackerSetting : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	TArray<FAssetHistoryEntry> Assets;
};
