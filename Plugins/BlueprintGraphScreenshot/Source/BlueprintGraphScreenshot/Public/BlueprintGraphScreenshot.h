// Copyright 2020 YeHaike. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetThumbnail.h"

class FToolBarBuilder;
class FMenuBuilder;

/**
 * The raw texture data from taking a screenshot of a Slate widget (typically the root window)
 */
struct FWidgetSnapshotTextureData
{
	/** The dimensions of the texture */
	FIntVector Dimensions;

	/** The raw color data for the texture (BGRA) */
	TArray<FColor> ColorData;
};

class FBlueprintGraphScreenshotModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command. */
	void PluginButtonClicked();

	void ButtonClickedForBlueprintGraphScreenshot();

	void TakeSnapshot();
	void CreateBrushes();
	void DestroyBrushes();
	void Reserve(const int32 NumWindows);
	void Reset();

	void CreateSnapshot(const TArray<TSharedRef<SWindow>>& VisibleWindows);

	TSharedRef<class SDockTab> CreateDefaultTab(const class FSpawnTabArgs& Args);
	
	static TArray<TSharedPtr<SWidget>> FindGraphEditorsOfCurrentStandaloneAssetEditorToolkit();
	static void RecursivelyFindChildrenWidgetOfType(TSharedPtr<SWidget> Widget, FString WidgetType, TArray<TSharedPtr<SWidget>>& OutFindedChildrenWidgets);

private:

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	void AddMenuExtensionForBlueprintGraphScreenshot(FMenuBuilder& Builder);
	void AddToolbarExtensionForBlueprintGraphScreenshot(FToolBarBuilder& Builder);

	void PrepareForBlueprintGraphScreenshot();
	void DoingBlueprintGraphScreenshot();

	void TakeScreenshot(TSharedPtr<SWidget> Widget, struct FWidgetSnapshotTextureData& TextureData);

	void OnPostTick(float DeltaTime);

private:
	TSharedPtr<class FUICommandList> PluginCommands;

	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	int32 CurrentThumbnailSize = 512;
	float ThumbnailScale = 1.0f;

	/** Contains a texture data entry for each entry in Windows */
	TArray<FWidgetSnapshotTextureData> WindowTextureData;

	/** Contains a dynamic brush pointer for each entry in WindowTextureData */
	TArray<TSharedPtr<struct FSlateDynamicImageBrush>> WindowTextureBrushes;

	bool TakingBlueprintGraphScreenshot = false;
};