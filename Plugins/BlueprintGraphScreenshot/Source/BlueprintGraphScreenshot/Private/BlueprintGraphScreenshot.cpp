// Copyright 2020 YeHaike. All Rights Reserved.

#include "BlueprintGraphScreenshot.h"
#include "BlueprintGraphScreenshotStyle.h"
#include "BlueprintGraphScreenshotCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"

#include "BlueprintEditorModule.h"
#include "MaterialEditorModule.h"

#include "Layout/ArrangedChildren.h"
#include "HighResScreenshot.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GraphEditor.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Docking/TabManager.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "ImagePixelData.h"

#include "LevelEditor.h"

static const FName BlueprintGraphScreenshotTabName("BlueprintGraphScreenshot");

#define LOCTEXT_NAMESPACE "FBlueprintGraphScreenshotModule"

#define MAX_VIEWPORT_SIZE 16200

void FBlueprintGraphScreenshotModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

#if WITH_EDITOR
	if (GIsEditor && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPostTick().AddRaw(this, &FBlueprintGraphScreenshotModule::OnPostTick);
	}
#endif

	FBlueprintGraphScreenshotStyle::Initialize();
	FBlueprintGraphScreenshotStyle::ReloadTextures();

	FBlueprintGraphScreenshotCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueprintGraphScreenshotCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FBlueprintGraphScreenshotModule::PluginButtonClicked),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FBlueprintGraphScreenshotCommands::Get().ActionForBlueprintGraphScreenshot,
		FExecuteAction::CreateRaw(this, &FBlueprintGraphScreenshotModule::ButtonClickedForBlueprintGraphScreenshot),
		FCanExecuteAction());

	{
		TSharedPtr<FExtender> ExtenderOfMenu = MakeShareable(new FExtender());
		ExtenderOfMenu->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FBlueprintGraphScreenshotModule::AddMenuExtensionForBlueprintGraphScreenshot));
		FAssetEditorToolkit::GetSharedMenuExtensibilityManager()->AddExtender(ExtenderOfMenu);

		TSharedPtr<FExtender> ExtenderOfToolBar = MakeShareable(new FExtender());
		ExtenderOfToolBar->AddToolBarExtension("Asset", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FBlueprintGraphScreenshotModule::AddToolbarExtensionForBlueprintGraphScreenshot));
		FAssetEditorToolkit::GetSharedToolBarExtensibilityManager()->AddExtender(ExtenderOfToolBar);
	}

	/*
	{
		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");

		{
			TSharedPtr<FExtender> ExtenderOfMenu = MakeShareable(new FExtender());
			ExtenderOfMenu->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FBlueprintGraphScreenshotModule::AddMenuExtensionForBlueprintGraphScreenshot));

			BlueprintEditorModule.GetMenuExtensibilityManager()->AddExtender(ExtenderOfMenu);
		}
	}
	*/
}

void FBlueprintGraphScreenshotModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FBlueprintGraphScreenshotStyle::Shutdown();

	FBlueprintGraphScreenshotCommands::Unregister();
}

void FBlueprintGraphScreenshotModule::PluginButtonClicked()
{

}

BEGIN_FUNCTION_BUILD_OPTIMIZATION

void FBlueprintGraphScreenshotModule::PrepareForBlueprintGraphScreenshot()
{
	TArray<TSharedPtr<SWidget>> GraphEditorsOfCurrentActiveTab = FindGraphEditorsOfCurrentStandaloneAssetEditorToolkit();

	int32 IndexOfWidget = -1;
	for (auto Widget : GraphEditorsOfCurrentActiveTab)
	{
		IndexOfWidget++;
		if (Widget.IsValid())
		{
			SGraphEditor* GraphEditor = static_cast<SGraphEditor*>(Widget.Get());
			if (GraphEditor)
			{
				FWidgetSnapshotTextureData& TextureData = WindowTextureData[WindowTextureData.AddDefaulted()];
				TakeScreenshot(Widget, TextureData);
			}
		}
	}
}

void FBlueprintGraphScreenshotModule::DoingBlueprintGraphScreenshot()
{
	TArray<TSharedPtr<SWidget>> GraphEditorsOfCurrentActiveTab = FindGraphEditorsOfCurrentStandaloneAssetEditorToolkit();

	Reset();
	Reserve(GraphEditorsOfCurrentActiveTab.Num());

	int32 IndexOfWidget = -1;
	for (auto Widget : GraphEditorsOfCurrentActiveTab)
	{
		IndexOfWidget++;
		if (Widget.IsValid())
		{
			SGraphEditor* GraphEditor = static_cast<SGraphEditor*>(Widget.Get());
			if (GraphEditor)
			{
				FWidgetSnapshotTextureData& TextureData = WindowTextureData[WindowTextureData.AddDefaulted()];
				TakeScreenshot(Widget, TextureData);

				FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
				int32 Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
				FPlatformTime::SystemTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);

				FString TimeAndIndex = FString::Printf(TEXT("_%04dY-%02dM-%02dD-%02dh-%02dm-%02ds-%03d_%02d"), Year, Month, Day, Hour, Min, Sec, MSec, IndexOfWidget);
				FString ImageSavePath = GetDefault<UEngine>()->GameScreenshotSaveDirectory.Path / TEXT("BPGraphScreenshots/BPGraphScreenshot") + TimeAndIndex;

				FString OutFilename = ImageSavePath + TEXT(".png");
				FIntPoint ImageSize(TextureData.Dimensions.X, TextureData.Dimensions.Y);
				// Save the bitmap to disc
				//HighResScreenshotConfig.SaveImage(ImageSavePath, TextureData.ColorData, ImageSize, &OutFilename);
				{
					TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();
					ImageTask->PixelData = MakeUnique<TImagePixelData<FColor>>(ImageSize, TArray64<FColor>(MoveTemp(TextureData.ColorData)));

					// Set full alpha on the bitmap
					bool bWriteAlpha = false;
					if (!bWriteAlpha)
					{
						ImageTask->PixelPreProcessors.Add(TAsyncAlphaWrite<FColor>(255));
					}

					HighResScreenshotConfig.PopulateImageTaskParams(*ImageTask);
					ImageTask->Filename = ImageSavePath;

					// Save the bitmap to disc
					TFuture<bool> CompletionFuture = HighResScreenshotConfig.ImageWriteQueue->Enqueue(MoveTemp(ImageTask));
					if (CompletionFuture.IsValid())
					{
						CompletionFuture.Wait();
					}
				}

				// Notification of a successful screenshot
				if (GIsEditor && !GIsAutomationTesting)
				{
					auto Message = NSLOCTEXT("BlueprintGraphScreenshot", "BlueprintGraphScreenshotSavedAs", "BlueprintGraph Screenshot saved as");
					FNotificationInfo Info(Message);
					Info.bFireAndForget = true;
					Info.ExpireDuration = 6.0f;
					Info.bUseSuccessFailIcons = true;
					Info.bUseLargeFont = true;
					Info.Image = FAppStyle::GetBrush(TEXT("NotificationList.SuccessImage"));

					const FString HyperLinkText = FPaths::ConvertRelativePathToFull(OutFilename);
					Info.Hyperlink = FSimpleDelegate::CreateStatic([](FString SourceFilePath)
						{
							FPlatformProcess::ExploreFolder(*(FPaths::GetPath(SourceFilePath)));
						}, HyperLinkText);
					Info.HyperlinkText = FText::FromString(HyperLinkText);

					FSlateNotificationManager::Get().AddNotification(Info);
				}
			}
		}
	}

	if (GraphEditorsOfCurrentActiveTab.Num() <= 0)
	{
		// Notification of a successful screenshot
		if (GIsEditor && !GIsAutomationTesting)
		{
			auto Message = NSLOCTEXT("BlueprintGraphScreenshot", "BlueprintGraphScreenshotFailed", "No GraphEditor. BlueprintGraph Screenshot Failed. ");
			FNotificationInfo Info(Message);
			Info.bFireAndForget = true;
			Info.ExpireDuration = 6.0f;
			Info.bUseSuccessFailIcons = true;
			Info.bUseLargeFont = true;
			Info.Image = FAppStyle::GetBrush(TEXT("NotificationList.FailImage"));

			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}

	CreateBrushes();
}

void FBlueprintGraphScreenshotModule::TakeScreenshot(TSharedPtr<SWidget> Widget, FWidgetSnapshotTextureData& TextureData)
{
	SGraphEditor* GraphEditor = static_cast<SGraphEditor*>(Widget.Get());
	if (!GraphEditor)
	{
		return;
	}

	FSlateRect BoundsForSelectedNodes;
	FGraphPanelSelectionSet  GraphPanelSelectionSet = GraphEditor->GetSelectedNodes();

	GraphEditor->GetBoundsForSelectedNodes(BoundsForSelectedNodes, 0.0f);
	FVector2D ViewLocation;
	FVector2D NewVewloction;
	float ZoomAmount;
	GraphEditor->GetViewLocation(ViewLocation, ZoomAmount);
	FGeometry CachedGeometry = GraphEditor->GetCachedGeometry();
	FVector2D SizeOfWidget = CachedGeometry.GetLocalSize();

	FVector2D TopLeftPadding = BoundsForSelectedNodes.GetTopLeft() - ViewLocation;

	FVector2D DefaultPaddingOfBoundsForSelectedNodes = FVector2D(100.0f, 100.0f);

	FVector2D PaddingOfWindowSize;
	PaddingOfWindowSize.X = TopLeftPadding.X > 0 ? TopLeftPadding.X * 2 : DefaultPaddingOfBoundsForSelectedNodes.X * 2;
	PaddingOfWindowSize.Y = TopLeftPadding.Y > 0 ? TopLeftPadding.Y * 2 : DefaultPaddingOfBoundsForSelectedNodes.Y * 2;

	NewVewloction.X = TopLeftPadding.X > 0 ? ViewLocation.X : BoundsForSelectedNodes.Left -100;
	NewVewloction.Y = TopLeftPadding.Y > 0 ? ViewLocation.Y : BoundsForSelectedNodes.Top - 100;

	GraphEditor->SetViewLocation(NewVewloction, ZoomAmount);

	float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(0.0f, 0.0f);

	FVector2D WindowSize = SizeOfWidget * DPIScale;
	if (GraphPanelSelectionSet.Num() > 0)
	{
		WindowSize = (BoundsForSelectedNodes.GetSize() + PaddingOfWindowSize) * DPIScale * ZoomAmount;
	}
	else
	{
		WindowSize = SizeOfWidget * DPIScale;
	}

	WindowSize = WindowSize.ClampAxes(0.0f, MAX_VIEWPORT_SIZE);

	GraphEditor->ClearSelectionSet();

	TSharedRef<SWindow> NewWindowRef = SNew(SWindow)
		.CreateTitleBar(false)
		.ClientSize(WindowSize)
		.ScreenPosition(FVector2D(0.0f, 0.0f))
		.AdjustInitialSizeAndPositionForDPIScale(false)
		.SaneWindowPlacement(false)
		.SupportsTransparency(EWindowTransparency::PerWindow)
		.InitialOpacity(0.0f);

	NewWindowRef->SetContent(Widget.ToSharedRef());

	const bool bShowImmediately = false;
	TSharedPtr<SWindow> WidgetWindow = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef());
	FSlateApplication::Get().AddWindow(NewWindowRef, bShowImmediately);

	GraphEditor->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	NewWindowRef->ShowWindow();

	// Screenshot the current window so we can pick against its current state
	FSlateApplication::Get().TakeScreenshot(NewWindowRef, TextureData.ColorData, TextureData.Dimensions);

	GraphEditor->SetViewLocation(ViewLocation, ZoomAmount);

	for (auto SelectedNode : GraphPanelSelectionSet)
	{
		if (UEdGraphNode* SelectedEdGraphNode = Cast<UEdGraphNode>(SelectedNode))
		{
			GraphEditor->SetNodeSelection(SelectedEdGraphNode, true);
		}
	}

	NewWindowRef->HideWindow();
	NewWindowRef->RequestDestroyWindow();
}

void FBlueprintGraphScreenshotModule::ButtonClickedForBlueprintGraphScreenshot()
{
	PrepareForBlueprintGraphScreenshot();
	TakingBlueprintGraphScreenshot = true;
	return;
}

void FBlueprintGraphScreenshotModule::OnPostTick(float DeltaTime)
{
	if (TakingBlueprintGraphScreenshot)
	{
		DoingBlueprintGraphScreenshot();
		TakingBlueprintGraphScreenshot = false;
	}
}

END_FUNCTION_BUILD_OPTIMIZATION

TArray<TSharedPtr<SWidget>> FBlueprintGraphScreenshotModule::FindGraphEditorsOfCurrentStandaloneAssetEditorToolkit()
{
	TArray<TSharedPtr<SWidget>> FindedChildrenWidgets;

	TSharedPtr<SDockTab> ActiveTab = FGlobalTabmanager::Get()->GetActiveTab();

	if (!ActiveTab.IsValid())
	{
		return FindedChildrenWidgets;
	}
	FWidgetPath WidgetPathOfActiveTab;
	TSharedRef<const SWidget> WidgetOfActiveTab = StaticCastSharedRef<SWidget>(ActiveTab.ToSharedRef());
	FSlateApplication::Get().FindPathToWidget(WidgetOfActiveTab, WidgetPathOfActiveTab, EVisibility::All);
	FArrangedChildren::FArrangedWidgetArray& ArrangedWidgetArray = WidgetPathOfActiveTab.Widgets.GetInternalArray();
	TSharedPtr<SWidget>  StandaloneAssetEditorToolkitHost = nullptr;
	for (auto ArrangedWidget : ArrangedWidgetArray)
	{
		TSharedPtr<SWidget> CurrentWidget = ArrangedWidget.Widget;
		if (CurrentWidget.IsValid())
		{
			FString TypeOfCurrentWidget = CurrentWidget->GetTypeAsString();
			if (TypeOfCurrentWidget.Equals(TEXT("SStandaloneAssetEditorToolkitHost")))
			{
				StandaloneAssetEditorToolkitHost = CurrentWidget;
				break;
			}
		}
	}

	if (!StandaloneAssetEditorToolkitHost.IsValid())
	{
		return FindedChildrenWidgets;
	}

	FString WidgetType = TEXT("SGraphEditorImpl");


	RecursivelyFindChildrenWidgetOfType(StandaloneAssetEditorToolkitHost, WidgetType, FindedChildrenWidgets);

	return FindedChildrenWidgets;
}

void FBlueprintGraphScreenshotModule::RecursivelyFindChildrenWidgetOfType(TSharedPtr<SWidget> Widget, FString WidgetType, TArray<TSharedPtr<SWidget>>& OutFindedChildrenWidgets)
{
	if (!Widget.IsValid())
	{
		return;
	}

	FChildren* Children = Widget->GetChildren();
	if (Children)
	{
		for (int32 i = 0; i < Children->Num(); i++)
		{
			TSharedPtr<SWidget> Child = Children->GetChildAt(i);
			if (Child.IsValid())
			{
				if (Child->GetTypeAsString().Equals(WidgetType))
				{
					OutFindedChildrenWidgets.AddUnique(Child);
				}
				RecursivelyFindChildrenWidgetOfType(Child, WidgetType, OutFindedChildrenWidgets);
			}
		}
	}
}

void FBlueprintGraphScreenshotModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FBlueprintGraphScreenshotCommands::Get().PluginAction);
}

void FBlueprintGraphScreenshotModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FBlueprintGraphScreenshotCommands::Get().PluginAction);
}

void FBlueprintGraphScreenshotModule::AddMenuExtensionForBlueprintGraphScreenshot(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FBlueprintGraphScreenshotCommands::Get().ActionForBlueprintGraphScreenshot);
}

void FBlueprintGraphScreenshotModule::AddToolbarExtensionForBlueprintGraphScreenshot(FToolBarBuilder& Builder)
{
	Builder.AddSeparator();
	Builder.AddToolBarButton(FBlueprintGraphScreenshotCommands::Get().ActionForBlueprintGraphScreenshot);
}

void FBlueprintGraphScreenshotModule::TakeSnapshot()
{
	TArray<TSharedRef<SWindow>> VisibleWindows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(VisibleWindows);

	CreateSnapshot(VisibleWindows);
}

void FBlueprintGraphScreenshotModule::Reserve(const int32 NumWindows)
{
	WindowTextureData.Reserve(NumWindows);
	WindowTextureBrushes.Reserve(NumWindows);
}

void FBlueprintGraphScreenshotModule::Reset()
{
	DestroyBrushes();

	//Windows.Reset();
	WindowTextureData.Reset();
	WindowTextureBrushes.Reset();
}

void FBlueprintGraphScreenshotModule::CreateSnapshot(const TArray<TSharedRef<SWindow>>& VisibleWindows)
{
	Reset();
	Reserve(VisibleWindows.Num());

	for (const auto& VisibleWindow : VisibleWindows)
	{
		// Screenshot the current window so we can pick against its current state
		FWidgetSnapshotTextureData& TextureData = WindowTextureData[WindowTextureData.AddDefaulted()];
		FSlateApplication::Get().TakeScreenshot(VisibleWindow, TextureData.ColorData, TextureData.Dimensions);
	}

	CreateBrushes();
}

void FBlueprintGraphScreenshotModule::CreateBrushes()
{
	DestroyBrushes();

	WindowTextureBrushes.Reserve(WindowTextureData.Num());

	static int32 TextureIndex = 0;
	for (const auto& TextureData : WindowTextureData)
	{
		if (TextureData.ColorData.Num() > 0)
		{
			TArray<uint8> TextureDataAsBGRABytes;
			TextureDataAsBGRABytes.Reserve(TextureData.ColorData.Num() * 4);
			for (const FColor& PixelColor : TextureData.ColorData)
			{
				TextureDataAsBGRABytes.Add(PixelColor.B);
				TextureDataAsBGRABytes.Add(PixelColor.G);
				TextureDataAsBGRABytes.Add(PixelColor.R);
				TextureDataAsBGRABytes.Add(PixelColor.A);
			}

			WindowTextureBrushes.Add(FSlateDynamicImageBrush::CreateWithImageData(
				*FString::Printf(TEXT("FWidgetSnapshotData_WindowTextureBrush_%d"), TextureIndex++),
				FVector2D(TextureData.Dimensions.X, TextureData.Dimensions.Y),
				TextureDataAsBGRABytes
			));
		}
		else
		{
			WindowTextureBrushes.Add(nullptr);
		}
	}
}

void FBlueprintGraphScreenshotModule::DestroyBrushes()
{
	for (const auto& WindowTextureBrush : WindowTextureBrushes)
	{
		if (WindowTextureBrush.IsValid())
		{
			WindowTextureBrush->ReleaseResource();
		}
	}

	WindowTextureBrushes.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintGraphScreenshotModule, BlueprintGraphScreenshot)