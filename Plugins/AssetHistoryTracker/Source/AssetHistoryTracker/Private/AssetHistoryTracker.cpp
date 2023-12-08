// Copyright 2023 X-Games. All Rights Reserved.

#include "AssetHistoryTracker.h"

#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "AssetHistoryTrackerStyle.h"
#include "AssetHistoryTrackerCommands.h"
#include "AssetHistoryTrackerPluginSettings.h"
#include "AssetHistoryTrackerSetting.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "WidgetBlueprint.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Views/SListView.h"
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#include "Engine/Font.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Internationalization/StringTable.h"
#include "Materials/MaterialInstance.h"
#include "Perception/AISense.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundSubmix.h"

static const FName AssetHistoryTrackerTabName("AssetHistoryTracker");

#define LOCTEXT_NAMESPACE "FAssetHistoryTrackerModule"

void FAssetHistoryTrackerModule::StartupModule()
{
	FAssetHistoryTrackerStyle::Initialize();
	FAssetHistoryTrackerStyle::ReloadTextures();
	FAssetHistoryTrackerCommands::Register();

	Commands = MakeShareable(new FUICommandList);
	Commands->MapAction(FAssetHistoryTrackerCommands::Get().OpenWindowCommand,
	                    FExecuteAction::CreateLambda([]()
	                    {
		                    FGlobalTabmanager::Get()->TryInvokeTab(AssetHistoryTrackerTabName);
	                    }),
	                    FCanExecuteAction());

	// Add Submenu
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAssetHistoryTrackerModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AssetHistoryTrackerTabName, FOnSpawnTab::CreateRaw(this, &FAssetHistoryTrackerModule::OnSpawnPluginTab))
	                        .SetDisplayName(LOCTEXT("AssetHistoryTrackerDisplayName", "Asset History Tracker"))
	                        .SetMenuType(ETabSpawnerMenuType::Enabled);


	// Register settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer("Editor");

		// General settings
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(
			"Editor", "Plugins", "AssetHistoryTracker",
			LOCTEXT("RuntimeGeneralSettingsNameDisplayName", "Asset History Tracker"),
			LOCTEXT("RuntimeGeneralSettingsDescription", "Configure the Asset History Tracker."),
			GetMutableDefault<UAssetHistoryTrackerPluginSettings>()
		);
	}


	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OnAssetEditorRequestedOpen().AddRaw(this, &FAssetHistoryTrackerModule::HandleAssetEditorOpenRequest);
}

void FAssetHistoryTrackerModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FAssetHistoryTrackerStyle::Shutdown();
	FAssetHistoryTrackerCommands::Unregister();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AssetHistoryTrackerTabName);

	// Unregister settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "AssetHistoryTracker");
	}
}

void FAssetHistoryTrackerModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntryWithCommandList(FAssetHistoryTrackerCommands::Get().OpenWindowCommand, Commands);
	}
}

void FAssetHistoryTrackerModule::HandleAssetEditorOpenRequest(UObject* Asset)
{
	if (!IsValid(Asset))
	{
		return;
	}

	// UE_LOG(LogTemp, Log, TEXT("HandleAssetEditorOpenRequest  AssetName=%s  ====>  ClassName=%s"), *Asset->GetName(), *Asset->GetClass()->GetName());

	if (Asset->GetClass() == ULevelScriptBlueprint::StaticClass())
	{
		return;
	}

	FString AssetPathName = Asset->GetPathName();
	FAssetHistoryEntry Entry = FAssetHistoryEntry(Asset->GetName(), AssetPathName, Asset->GetClass());

	TArray<FAssetHistoryEntry>& RecentList = GetMutableDefault<UAssetHistoryTrackerSetting>()->Assets;

	const int32 Index = RecentList.IndexOfByPredicate([AssetPathName](const FAssetHistoryEntry& Entry)
	{
		return Entry.AssetPathName == AssetPathName;
	});
	if (Index != INDEX_NONE && Index != 0)
	{
		RecentList.RemoveAt(Index);
	}
	if (Index == INDEX_NONE || Index != 0)
	{
		RecentList.EmplaceAt(0, Entry);
	}

	const int MaxHistoryCapacity = GetDefault<UAssetHistoryTrackerPluginSettings>()->Capacity;
	while (RecentList.Num() > MaxHistoryCapacity)
	{
		RecentList.RemoveAt(RecentList.Num() - 1);
	}

	GetMutableDefault<UAssetHistoryTrackerSetting>()->SaveConfig();

	RefreshListView(CurrentClass);
}

TSharedRef<SDockTab> FAssetHistoryTrackerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	InitMenus();
	const auto Content = MakeDockTabContent();
	auto DockTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	DockTab.Get().SetTabIcon(FSlateIconFinder::FindIconForClass(ULevel::StaticClass()).GetIcon());
	DockTab->SetContent(Content);

	if (Menus.Num() > 0)
	{
		MenuListView->SetItemSelection(Menus[0], true);
	}
	return DockTab;
}


TSharedRef<SWidget> FAssetHistoryTrackerModule::MakeDockTabContent()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		  .FillHeight(1)
		  .HAlign(HAlign_Fill)
		  .VAlign(VAlign_Fill)
		  .Padding(FMargin(2.0f, 2.0f, 2.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			  .FillWidth(0.2)
			  .MaxWidth(300)
			[
				SAssignNew(MenuListView, SListView<TSharedPtr<UClass*>>)
					   .ItemHeight(200.0f)
					   .ListItemsSource(&Menus)
					   .SelectionMode(ESelectionMode::Single)
					   .OnGenerateRow_Raw(this, &FAssetHistoryTrackerModule::OnGenerateMenuRow)
					   .Visibility(EVisibility::Visible)
					   .OnSelectionChanged_Lambda([this](const TSharedPtr<UClass*>& ClassPtr, ESelectInfo::Type SelectType)
				                                                        {
					                                                        if (ClassPtr.IsValid())
					                                                        {
						                                                        RefreshListView(*ClassPtr.Get());
						                                                        if (RecentlyUObject.Num() > 0)
						                                                        {
							                                                        CurrentUObjectName = FText::FromString(*RecentlyUObject[0].Get()->AssetPathName);
						                                                        }
						                                                        else
						                                                        {
							                                                        CurrentUObjectName = FText::FromString(TEXT(""));
						                                                        }
					                                                        }
				                                                        })
			]
			+ SHorizontalBox::Slot()
			  .Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			  .FillWidth(0.8)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FAssetHistoryEntry>>)
				   .ItemHeight(200.0f)
				   .ListItemsSource(&RecentlyUObject)
				   .SelectionMode(ESelectionMode::Single)
				   .OnGenerateRow_Raw(this, &FAssetHistoryTrackerModule::OnGenerateRow)
				   .Visibility(EVisibility::Visible)
				   .OnSelectionChanged_Lambda([this](const TSharedPtr<FAssetHistoryEntry>& EntryPtr, ESelectInfo::Type SelectType)
				                                                          {
					                                                          if (EntryPtr.IsValid())
					                                                          {
						                                                          const FAssetHistoryEntry Entry = *EntryPtr.Get();
						                                                          CurrentUObjectName = FText::FromString(Entry.AssetPathName);
					                                                          }
				                                                          })
			]
		] + SVerticalBox::Slot()
		    .Padding(FMargin(2.0f, 2.0f, 2.0f, 2.0f))
		    .AutoHeight()
		[
			SNew(SBorder)
			.ColorAndOpacity(FLinearColor::Gray)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return CurrentUObjectName;
				})
			]
		];
}

TSharedRef<ITableRow> FAssetHistoryTrackerModule::OnGenerateMenuRow(TSharedPtr<UClass*> ClassPtr, const TSharedRef<STableViewBase>& OwnerTable) const
{
	UClass* Class = *ClassPtr.Get();
	auto MakeContent = [Class,this]()-> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.HeightOverride(25)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				  .HAlign(HAlign_Left)
				  .VAlign(VAlign_Center)
				  .AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(15, 0))
				]
				+ SHorizontalBox::Slot()
				  .HAlign(HAlign_Left)
				  .VAlign(VAlign_Center)
				  .AutoWidth()
				[
					SNew(SImage)
					.Image(FSlateIconFinder::FindIconForClass(Class).GetIcon())
				]
				+ SHorizontalBox::Slot()
				  .HAlign(HAlign_Left)
				  .VAlign(VAlign_Center)
				  .AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(5, 0))
				]
				+ SHorizontalBox::Slot()
				  .HAlign(HAlign_Left)
				  .VAlign(VAlign_Center)
				  .AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this, Class]()
					{
						FText MenuName = Class == UObject::StaticClass() ? FText::FromString(TEXT("All")) : FText::FromString(*Class->GetName());

						if (UObjectCountMap.Contains(Class))
						{
							return FText::Format(LOCTEXT("MenuFormat", "{0}({1})"), MenuName, UObjectCountMap[Class]);
						}

						return MenuName;
					})
				]
			];
	};

	auto TableRow = SNew(STableRow<TSharedPtr<UClass*>>, OwnerTable);
	TableRow->SetContent(MakeContent());

	return TableRow;
}

TSharedRef<ITableRow> FAssetHistoryTrackerModule::OnGenerateRow(TSharedPtr<FAssetHistoryEntry> EntryPtr, const TSharedRef<STableViewBase>& OwnerTable) const
{
	if (!EntryPtr.IsValid())
	{
		auto TableRow = SNew(STableRow<TSharedPtr<FAssetHistoryEntry>>, OwnerTable);
		return TableRow;
	}

	auto MakeContent = [EntryPtr,this]()-> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.HeightOverride(25)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				  .HAlign(HAlign_Left)
				  .VAlign(VAlign_Center)
				  .AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(15, 0))
				]
				+ SHorizontalBox::Slot()
				  .HAlign(HAlign_Left)
				  .VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(*EntryPtr.Get()->AssetName))
				]
				+ SHorizontalBox::Slot()
				  .HAlign(HAlign_Right)
				  .VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("Open", "Open"))
					.OnClicked_Lambda([EntryPtr]()
					             {
						             GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(*EntryPtr.Get()->AssetPathName);
						             return FReply::Handled();
					             })
				]
			];
	};

	auto TableRow = SNew(STableRow<TSharedPtr<FAssetHistoryEntry>>, OwnerTable);
	TableRow->SetContent(MakeContent());
	return TableRow;
}

void FAssetHistoryTrackerModule::RefreshListView(UClass* Class)
{
	this->CurrentClass = Class;
	RecentlyUObject.Empty();
	UObjectCountMap.Empty();
	TArray<FAssetHistoryEntry>& List = GetMutableDefault<UAssetHistoryTrackerSetting>()->Assets;
	TArray<FAssetHistoryEntry> NewList;
	int32 UObjectCount = 0;
	for (const auto& Entry : List)
	{
		if (FPackageName::DoesPackageExist(Entry.AssetPathName))
		{
			NewList.Add(Entry);
			UObjectCount++;

			if (Class == UObject::StaticClass() || Entry.Class == Class)
			{
				TSharedPtr<FAssetHistoryEntry> EntryPtr = MakeShared<FAssetHistoryEntry>(Entry);
				RecentlyUObject.Add(EntryPtr);
			}

			int32 Count;
			if (!UObjectCountMap.Contains(Entry.Class))
			{
				Count = 0;
			}
			else
			{
				Count = UObjectCountMap[Entry.Class];
			}
			Count++;
			UObjectCountMap.Emplace(Entry.Class, Count);
		}
	}
	if (UObjectCount > 0)
	{
		UObjectCountMap.Emplace(UObject::StaticClass(), UObjectCount);
	}

	if (List.Num() != NewList.Num())
	{
		List = MoveTemp(NewList);
		GetMutableDefault<UAssetHistoryTrackerSetting>()->SaveConfig();
	}

//++ CK
	// ListView->RequestListRefresh();
	if (ListView.IsValid())
	{ ListView->RequestListRefresh(); }
//-- CK
}

void FAssetHistoryTrackerModule::InitMenus()
{
	Menus.Empty();

	Menus.Add(MakeShared<UClass*>(UObject::StaticClass()));

#define ADD_CLASS_MENU(ClassName) \
	if (GetDefault<UAssetHistoryTrackerPluginSettings>()->ClassName) \
	{ \
	Menus.Add(MakeShared<UClass*>(U##ClassName::StaticClass())); \
	}
	// blueprint
	ADD_CLASS_MENU(Blueprint);
	ADD_CLASS_MENU(WidgetBlueprint);
	ADD_CLASS_MENU(AnimBlueprint);

	Menus.Add(MakeShared<UClass*>(UTexture2D::StaticClass()));
	Menus.Add(MakeShared<UClass*>(UDataTable::StaticClass()));
	Menus.Add(MakeShared<UClass*>(UStringTable::StaticClass()));
	Menus.Add(MakeShared<UClass*>(UStaticMesh::StaticClass()));

	// user defined
	ADD_CLASS_MENU(UserDefinedEnum);
	ADD_CLASS_MENU(UserDefinedStruct);
	// material
	ADD_CLASS_MENU(Material);
	ADD_CLASS_MENU(MaterialInstance);
	ADD_CLASS_MENU(MaterialFunction);
	// animation
	ADD_CLASS_MENU(AnimMontage);
	ADD_CLASS_MENU(AnimSequence);
	ADD_CLASS_MENU(AnimationAsset);
	// sound
	ADD_CLASS_MENU(SoundWave);
	ADD_CLASS_MENU(SoundCue);
	ADD_CLASS_MENU(SoundAttenuation);
	ADD_CLASS_MENU(SoundMix);
	ADD_CLASS_MENU(SoundSubmix);
	// curve
	ADD_CLASS_MENU(CurveTable);
	ADD_CLASS_MENU(CurveFloat);
	ADD_CLASS_MENU(CurveLinearColor);
	ADD_CLASS_MENU(CurveVector);
	// Skeleton
	ADD_CLASS_MENU(Skeleton);
	ADD_CLASS_MENU(SkeletalMesh);
	// AI
	ADD_CLASS_MENU(BehaviorTree);
	ADD_CLASS_MENU(BlackboardData);
	ADD_CLASS_MENU(BlackboardComponent);
	ADD_CLASS_MENU(AISense);
	// other
	ADD_CLASS_MENU(Font);
	ADD_CLASS_MENU(PhysicsAsset);
}


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAssetHistoryTrackerModule, AssetHistoryTracker)
