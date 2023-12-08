// Copyright 2023 X-Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetHistoryTrackerSetting.h"

class FToolBarBuilder;
class FMenuBuilder;

class FAssetHistoryTrackerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

private:
	UClass* CurrentClass = UObject::StaticClass();

	FText CurrentUObjectName;

	TSharedPtr<FUICommandList> Commands;

	TArray<TSharedPtr<UClass*>> Menus;

	TMap<UClass*, int32> UObjectCountMap;

	TSharedPtr<SListView<TSharedPtr<UClass*>>> MenuListView;

	TArray<TSharedPtr<FAssetHistoryEntry>> RecentlyUObject;

	TSharedPtr<SListView<TSharedPtr<FAssetHistoryEntry>>> ListView;

	void RegisterMenus();

	void HandleAssetEditorOpenRequest(UObject* Asset);

	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);

	TSharedRef<SWidget> MakeDockTabContent();

	TSharedRef<ITableRow> OnGenerateMenuRow(TSharedPtr<UClass*> ClassPtr, const TSharedRef<STableViewBase>& OwnerTable) const;

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FAssetHistoryEntry> EntryPtr, const TSharedRef<STableViewBase>& OwnerTable) const;
	
	void RefreshListView(UClass* Class);

	void InitMenus();
};
