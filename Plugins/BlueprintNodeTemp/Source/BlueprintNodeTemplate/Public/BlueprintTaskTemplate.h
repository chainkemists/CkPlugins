//	Copyright 2020 Alexandr Marchenko. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "NameSelect.h"
#include "UObject/Object.h"

#include "BlueprintTaskTemplate.generated.h"


class UWorld;

/** BlueprintTaskTemplate */
UCLASS(Abstract, Blueprintable, BlueprintType)
class BLUEPRINTNODETEMPLATE_API UBlueprintTaskTemplate : public UObject
{
	GENERATED_BODY()
public:
	UBlueprintTaskTemplate(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(
		BlueprintCallable,
		Category = "BlueprintTaskTemplate",
		meta =
			(DisplayName = "BlueprintTaskTemplate",
			 DefaultToSelf = "Outer",
			 BlueprintInternalUseOnly = "TRUE",
			 DeterminesOutputType = "Class",
			 Keywords = "BP Blueprint Task Template"))
	static UBlueprintTaskTemplate* BlueprintTaskTemplate(UObject* Outer, TSubclassOf<UBlueprintTaskTemplate> Class);

	UFUNCTION(BlueprintCallable, Category = "BlueprintTaskTemplate", meta = (DisplayName = "Activate", ExposeAutoCall = "true"))
	void Activate() { Activate_Internal(); }

	virtual UWorld* GetWorld() const override { return WorldPrivate; }
	virtual void BeginDestroy() override;

	virtual void Serialize(FArchive& Ar) override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "BlueprintTaskTemplate", meta = (DisplayName = "Activate"))
	void Activate_BP();
	virtual void Activate_Internal()
	{
		if (GetOuter())
		{
			Activate_BP();
		}
	}

private:
	UPROPERTY(Transient)
	UWorld* WorldPrivate = nullptr;

public:
	UPROPERTY(EditDefaultsOnly, Category = "DisplayOptions")
	FName Category = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "DisplayOptions")
	FName Tooltip = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "DisplayOptions")
	FName MenuDisplayName = NAME_None;

#if WITH_EDITOR
public:
	// force refresh all for old/new in-editor params/functions etc
	void RefreshCollected();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;


protected:
	static void CollectSpawnParam(const UClass* InClass, TSet<FName>& Out);
	static void CollectFunctions(const UClass* InClass, TSet<FName>& Out);
	static void CollectDelegates(const UClass* InClass, TSet<FName>& Out);
	static void CleanInvalidParams(TArray<FNameSelect>& Arr, const TSet<FName>& ArrRef);

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
public:
	UPROPERTY(VisibleDefaultsOnly, Category = "ExposeOptions")
	TSet<FName> AllDelegates;
	UPROPERTY(VisibleDefaultsOnly, Category = "ExposeOptions")
	TSet<FName> AllFunctions;
	UPROPERTY(VisibleDefaultsOnly, Category = "ExposeOptions")
	TSet<FName> AllFunctionsExec;
	UPROPERTY(VisibleDefaultsOnly, Category = "ExposeOptions")
	TSet<FName> AllParam;

	UPROPERTY(EditDefaultsOnly, Category = "ExposeOptions")
	TArray<FNameSelect> SpawnParam;
	UPROPERTY(EditDefaultsOnly, Category = "ExposeOptions")
	TArray<FNameSelect> AutoCallFunction;
	UPROPERTY(EditDefaultsOnly, Category = "ExposeOptions")
	TArray<FNameSelect> ExecFunction;
	UPROPERTY(EditDefaultsOnly, Category = "ExposeOptions")
	TArray<FNameSelect> InDelegate;
	UPROPERTY(EditDefaultsOnly, Category = "ExposeOptions")
	TArray<FNameSelect> OutDelegate;
#endif
};
