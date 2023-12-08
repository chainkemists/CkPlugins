// Copyright 2023 X-Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "AssetHistoryTrackerStyle.h"

class FAssetHistoryTrackerCommands : public TCommands<FAssetHistoryTrackerCommands>
{
public:

	FAssetHistoryTrackerCommands()
		: TCommands<FAssetHistoryTrackerCommands>(TEXT("AssetHistoryTracker"), NSLOCTEXT("Contexts", "AssetHistoryTracker", "AssetHistoryTracker Plugin"), NAME_None, FAssetHistoryTrackerStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenWindowCommand;
};