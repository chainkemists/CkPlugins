//	Copyright 2020 Alexandr Marchenko. All Rights Reserved.

#include "BlueprintTaskTemplate.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "BlueprintNodeTemplate.h"
#include "ExtendConstructObject_FnLib.h"


#if WITH_EDITOR
	#include "Interfaces/IPluginManager.h"
	#include "ObjectEditorUtils.h"
#endif // WITH_EDITOR


UBlueprintTaskTemplate::UBlueprintTaskTemplate(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	if (const UObject* Owner = GetOuter())
	{
		WorldPrivate = Owner->GetWorld();
	}
}

UBlueprintTaskTemplate* UBlueprintTaskTemplate::BlueprintTaskTemplate(UObject* Outer, const TSubclassOf<UBlueprintTaskTemplate> Class)
{
	if (IsValid(Outer) && Class && !Class->HasAnyClassFlags(CLASS_Abstract))
	{
		const FName TaskObjName = MakeUniqueObjectName(Outer, Class, Class->GetFName()); //Class->GetFName();//
		const auto Task = NewObject<UBlueprintTaskTemplate>(Outer, Class, TaskObjName, RF_NoFlags);
		return Task;
	}
	return nullptr;
}

void UBlueprintTaskTemplate::BeginDestroy()
{
	WorldPrivate = nullptr;
	Super::BeginDestroy();
}

void UBlueprintTaskTemplate::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
#if WITH_EDITOR
	Ar.UsingCustomVersion(FBlueprintNodeTemplateCustomVersion::GUID);
	if (Ar.IsLoading() && GetLinkerCustomVersion(FBlueprintNodeTemplateCustomVersion::GUID) < FBlueprintNodeTemplateCustomVersion::ExposeOnSpawnInClass)
	{
		RefreshCollected();
		const FSpawnParam Spawn = UExtendConstructObject_FnLib::CollectSpawnParam(GetClass(), AllDelegates, AllFunctions, AllFunctionsExec, AllParam);
		for (const auto& It : Spawn.AutoCallFunction)
		{
			AutoCallFunction.AddUnique(It);
		}
		for (const auto& It : Spawn.ExecFunction)
		{
			ExecFunction.AddUnique(It);
		}
		for (const auto& It : Spawn.InDelegate)
		{
			InDelegate.AddUnique(It);
		}
		for (const auto& It : Spawn.OutDelegate)
		{
			OutDelegate.AddUnique(It);
		}
		for (const auto& It : Spawn.SpawnParam)
		{
			SpawnParam.AddUnique(It);
		}
	}
#endif
}


#if WITH_EDITOR
void UBlueprintTaskTemplate::CollectSpawnParam(const UClass* InClass, TSet<FName>& Out)
{
	Out.Reset();
	for (TFieldIterator<FProperty> PropertyIt(InClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		const FProperty* Property = *PropertyIt;
		const bool bIsDelegate = Property->IsA(FMulticastDelegateProperty::StaticClass());
		const bool bIsExposedToSpawn = Property->HasMetaData(TEXT("ExposeOnSpawn")) || Property->HasAllPropertyFlags(CPF_ExposeOnSpawn);
		const bool bIsSettableExternally = !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);

		if (!Property->HasAnyPropertyFlags(CPF_Parm) && !bIsDelegate)
		{
			if (bIsExposedToSpawn && bIsSettableExternally && Property->HasAllPropertyFlags(CPF_BlueprintVisible))
			{
				Out.Add(Property->GetFName());
			}
			else if (
				!Property->HasAnyPropertyFlags(			 //
					CPF_NativeAccessSpecifierProtected | //
					CPF_NativeAccessSpecifierPrivate |	 //
					CPF_Protected |						 //
					CPF_BlueprintReadOnly |				 //
					CPF_EditorOnly |					 //
					CPF_InstancedReference |			 //
					CPF_Deprecated |					 //
					CPF_ExportObject) &&				 //
				Property->HasAllPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				Out.Add(Property->GetFName());
			}
		}
	}
}
void UBlueprintTaskTemplate::CollectFunctions(const UClass* InClass, TSet<FName>& Out)
{
	Out.Reset();
	for (TFieldIterator<UField> It(InClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		if (const UFunction* LocFunction = Cast<UFunction>(*It))
		{
			if (LocFunction->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_Public) && //
				!LocFunction->GetBoolMetaData(FName(TEXT("BlueprintInternalUseOnly"))) && //
				// ++CK Autocall functions should not display in the list of exec functions since they will
				// revert to 'None' on save anyway
				!LocFunction->GetBoolMetaData(FName(TEXT("ExposeAutoCall"))) &&           //
				// --CK
				!LocFunction->HasMetaData(FName(TEXT("DeprecatedFunction"))) &&			  //
				!FObjectEditorUtils::IsFunctionHiddenFromClass(LocFunction, InClass) &&	  //!InClass->IsFunctionHidden(*LocFunction->GetName())
				!LocFunction->HasAnyFunctionFlags(										  //
					FUNC_Static |														  //
					FUNC_UbergraphFunction |											  //
					FUNC_Delegate |														  //
					FUNC_Private |														  //
					FUNC_Protected |													  //
					FUNC_EditorOnly |													  //
					FUNC_BlueprintPure |												  //
					FUNC_Const))														  // FUNC_BlueprintPure
			{
				Out.Add(LocFunction->GetFName());
			}
		}
	}
}
void UBlueprintTaskTemplate::CollectDelegates(const UClass* InClass, TSet<FName>& Out)
{
	Out.Reset();
	for (TFieldIterator<FProperty> PropertyIt(InClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		if (const FMulticastDelegateProperty* DelegateProperty = CastField<FMulticastDelegateProperty>(*PropertyIt))
		{
			if (DelegateProperty->HasAnyPropertyFlags(FUNC_Private | CPF_Protected | FUNC_EditorOnly) || //
				!DelegateProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable) ||						 //
				DelegateProperty->GetBoolMetaData(FName(TEXT("BlueprintInternalUseOnly"))) ||			 //
				DelegateProperty->HasMetaData(TEXT("DeprecatedFunction")))
			{
				continue;
			}

			if (const UFunction* LocFunction = DelegateProperty->SignatureFunction)
			{
				if (!LocFunction->HasAllFunctionFlags(FUNC_Public) ||				  //
					LocFunction->GetBoolMetaData(TEXT("BlueprintInternalUseOnly")) || //
					LocFunction->HasMetaData(TEXT("DeprecatedFunction")) ||
					LocFunction->HasAnyFunctionFlags( //
						FUNC_Static |				  //
						FUNC_BlueprintPure |		  //
						FUNC_Const |				  //
						FUNC_UbergraphFunction |	  //
						FUNC_Private |				  //
						FUNC_Protected |			  //
						FUNC_EditorOnly))
				{
					continue;
				}
			}
			Out.Add(DelegateProperty->GetFName());
		}
	}
}
void UBlueprintTaskTemplate::CleanInvalidParams(TArray<FNameSelect>& Arr, const TSet<FName>& ArrRef)
{
	for (int32 i = Arr.Num() - 1; i >= 0; --i)
	{
		if (Arr[i].Name != NAME_None && !ArrRef.Contains(Arr[i]))
		{
			Arr.RemoveAt(i, 1, false);
		}
	}
}

void UBlueprintTaskTemplate::RefreshCollected()
{

	#if WITH_EDITORONLY_DATA
	{
		const UClass* InClass = GetClass();
		CollectSpawnParam(InClass, AllParam);
		CollectFunctions(InClass, AllFunctions);
		CollectDelegates(InClass, AllDelegates);
		AllFunctionsExec = AllFunctions;
		CleanInvalidParams(AutoCallFunction, AllFunctions);
		CleanInvalidParams(ExecFunction, AllFunctions);
		CleanInvalidParams(InDelegate, AllDelegates);
		CleanInvalidParams(OutDelegate, AllDelegates);
		CleanInvalidParams(SpawnParam, AllParam);
		AutoCallFunction.AddUnique(FName(TEXT("Activate")));
		for (auto& It : AutoCallFunction)
		{
			It.SetAllExclude(AllFunctions, AutoCallFunction);
			ExecFunction.Remove(It);
		}
		for (auto& It : ExecFunction)
		{
			It.SetAllExclude(AllFunctionsExec, ExecFunction);
			AutoCallFunction.Remove(It);
		}
		for (auto& It : InDelegate)
		{
			It.SetAllExclude(AllDelegates, InDelegate);
			OutDelegate.Remove(It);
		}
		for (auto& It : OutDelegate)
		{
			It.SetAllExclude(AllDelegates, OutDelegate);
			InDelegate.Remove(It);
		}
		for (auto& It : SpawnParam)
		{
			It.SetAllExclude(AllParam, SpawnParam);
		}
		AutoCallFunction.AddUnique(FName(TEXT("Activate")));

		// ++CK
		AutoCallFunction.Remove(FName(TEXT("Deactivate")));
		ExecFunction.AddUnique(FName(TEXT("Deactivate")));
		// --CK
	}
	#endif // WITH_EDITORONLY_DATA
}

void UBlueprintTaskTemplate::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshCollected();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR