// Copyright 2023 X-Games. All Rights Reserved.

#include "AssetHistoryTrackerCommands.h"

#define LOCTEXT_NAMESPACE "FAssetHistoryTrackerModule"

void FAssetHistoryTrackerCommands::RegisterCommands()
{
	UI_COMMAND(OpenWindowCommand, "Asset History Tracker", "Bring up AssetHistoryTracker window", EUserInterfaceActionType::Button, FInputGesture());

}

#undef LOCTEXT_NAMESPACE
