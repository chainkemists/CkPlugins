// Copyright 2020 YeHaike. All Rights Reserved.

#include "BlueprintGraphScreenshotCommands.h"

#define LOCTEXT_NAMESPACE "FBlueprintGraphScreenshotModule"
 
void FBlueprintGraphScreenshotCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "BlueprintGraphScreenshot", "Execute BlueprintGraphScreenshot action", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ActionForBlueprintGraphScreenshot, "GraphShot", "GraphScreenshot: Take screenshots of selected Nodes in Graphs. Steps: 1. Select Nodes; 2. Scale them to 1:1; 3. Align to the Top-Left Corner; 4. Click this button.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
