// Copyright 2020 YeHaike. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "BlueprintGraphScreenshotStyle.h"

class FBlueprintGraphScreenshotCommands : public TCommands<FBlueprintGraphScreenshotCommands>
{
public:

	FBlueprintGraphScreenshotCommands()
		: TCommands<FBlueprintGraphScreenshotCommands>(TEXT("BlueprintGraphScreenshot"), NSLOCTEXT("Contexts", "BlueprintGraphScreenshot", "BlueprintGraphScreenshot Plugin"), NAME_None, FBlueprintGraphScreenshotStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:

	TSharedPtr< FUICommandInfo > PluginAction;
	TSharedPtr< FUICommandInfo > ActionForBlueprintGraphScreenshot;
};
