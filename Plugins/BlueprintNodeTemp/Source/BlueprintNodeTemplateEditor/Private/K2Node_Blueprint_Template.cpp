//	Copyright 2020 Alexandr Marchenko. All Rights Reserved.

#include "K2Node_Blueprint_Template.h"

#include "BlueprintTaskTemplate.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"


#define LOCTEXT_NAMESPACE "K2Node"


UK2Node_Blueprint_Template::UK2Node_Blueprint_Template(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UBlueprintTaskTemplate, BlueprintTaskTemplate);
	ProxyFactoryClass = UBlueprintTaskTemplate::StaticClass();
	OutPutObjectPinName = FName(TEXT("TaskObject"));
	//AutoCallFunctions.Add(GET_FUNCTION_NAME_CHECKED(UBlueprintTaskTemplate, Init_Activate));
}

void UK2Node_Blueprint_Template::HideClassPin() const
{
	UEdGraphPin* ClassPin = FindPinChecked(ClassPinName);
	ClassPin->DefaultObject = ProxyClass;
	ClassPin->DefaultValue.Empty();
	ClassPin->bDefaultValueIsReadOnly = true;
	ClassPin->bNotConnectable = true;
	ClassPin->bHidden = true;
}

void UK2Node_Blueprint_Template::RegisterBlueprintAction(UClass* TargetClass, FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const FString Name = TargetClass->GetName();

	if (!TargetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) && !Name.Contains(FNames_Helper::SkelPrefix) &&
		!Name.Contains(FNames_Helper::ReinstPrefix) && !Name.Contains(FNames_Helper::DeadclassPrefix))
	{
		UClass* NodeClass = GetClass();
		auto Lambda = [NodeClass, TargetClass](const UFunction* FactoryFunc) -> UBlueprintNodeSpawner*
		{
			auto CustomizeTimelineNodeLambda = [TargetClass](UEdGraphNode* NewNode, bool bIsTemplateNode, const TWeakObjectPtr<UFunction> FunctionPtr)
			{
				UK2Node_Blueprint_Template* AsyncTaskNode = CastChecked<UK2Node_Blueprint_Template>(NewNode);
				if (FunctionPtr.IsValid())
				{
					UFunction* Func = FunctionPtr.Get();
					AsyncTaskNode->ProxyFactoryFunctionName = Func->GetFName();
					//AsyncTaskNode->ProxyFactoryClass = Func->GetOuterUClass();
					AsyncTaskNode->ProxyClass = TargetClass;
				}
			};

			UBlueprintNodeSpawner* NodeSpawner = UBlueprintFunctionNodeSpawner::Create(FactoryFunc);
			NodeSpawner->NodeClass = NodeClass;
			if (TargetClass && TargetClass->HasMetaData(TEXT("Category")))
			{
				NodeSpawner->DefaultMenuSignature.Category = FText::FromString(TargetClass->GetMetaData(TEXT("Category")));
			}

			const TWeakObjectPtr<UFunction> FunctionPtr = MakeWeakObjectPtr(const_cast<UFunction*>(FactoryFunc));
			FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;

			if (TargetClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
			{
				FString LocName = TargetClass->GetName();
				LocName.RemoveFromEnd(FNames_Helper::CompiledFromBlueprintSuffix);

				if (const UBlueprintTaskTemplate* TargetClassAsBlueprintTask = Cast<UBlueprintTaskTemplate>(TargetClass->ClassDefaultObject))
				{
					if (TargetClassAsBlueprintTask->Category != NAME_None)
					{
				        MenuSignature.Category = FText::FromName(TargetClassAsBlueprintTask->Category);
					}

					if (TargetClassAsBlueprintTask->Tooltip != NAME_None)
					{
				        MenuSignature.Tooltip = FText::FromName(TargetClassAsBlueprintTask->Tooltip);
					}

				    MenuSignature.MenuName = TargetClassAsBlueprintTask->MenuDisplayName != NAME_None
				                                ? FText::FromName(TargetClassAsBlueprintTask->MenuDisplayName)
				                                : FText::FromString(LocName);

					MenuSignature.Keywords = MenuSignature.MenuName;
				}
				else
				{
				    MenuSignature.MenuName = FText::FromString(LocName);
			        MenuSignature.Keywords = FText::FromString(LocName);
				}
			}

			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda(CustomizeTimelineNodeLambda, FunctionPtr);
			return NodeSpawner;
		};

		/*if (const UObject* RegistrarTarget = ActionRegistrar.GetActionKeyFilter()) //... todo: RegisterBlueprintAction ActionRegistrar.GetActionKeyFilter()
		{
			if (const UClass* TarClass = Cast<UClass>(RegistrarTarget))
			{
				if (!TarClass->HasAnyClassFlags(CLASS_Abstract) && !TarClass->IsChildOf(TargetClass))
				{
					for (TFieldIterator<UFunction> FuncIt(TarClass); FuncIt; ++FuncIt)
					{
						UFunction* Function = *FuncIt;
						if (!Function->HasAnyFunctionFlags(FUNC_Static)) continue;
						if (CastField<FObjectProperty>(Function->GetReturnProperty()) == nullptr) continue;

						if (UBlueprintNodeSpawner* NewAction = Lambda(Function))
						{
							ActionRegistrar.AddBlueprintAction(Function, NewAction);
						}
					}
				}
			}
		}
		else*/
		{
			for (TFieldIterator<UFunction> FuncIt(TargetClass); FuncIt; ++FuncIt)
			{
				const UFunction* const Function = *FuncIt;
				if (!Function->HasAnyFunctionFlags(FUNC_Static)) continue;
				if (CastField<FObjectProperty>(Function->GetReturnProperty()) == nullptr) continue;

				if (UBlueprintNodeSpawner* NewAction = Lambda(Function))
				{
					ActionRegistrar.AddBlueprintAction(Function, NewAction);
				}
			}
		}
	}
}

void UK2Node_Blueprint_Template::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
	check(GetWorldContextPin());
	HideClassPin();

	UK2Node::AllocateDefaultPins();
	GetGraph()->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
}

void UK2Node_Blueprint_Template::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);
	check(GetWorldContextPin());
	HideClassPin();

	UK2Node::AllocateDefaultPins();
	GetGraph()->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
}


void UK2Node_Blueprint_Template::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* TargetClass = *It;
		if (TargetClass->IsChildOf(ProxyFactoryClass))
		{
			RegisterBlueprintAction(TargetClass, ActionRegistrar);
		}
	}
}

void UK2Node_Blueprint_Template::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
}

FText UK2Node_Blueprint_Template::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (ProxyClass)
	{
		const FString Str = ProxyClass->GetName();
		TArray<FString> ParseNames;
		Str.ParseIntoArray(ParseNames, TEXT("_C"));
		return FText::FromString(ParseNames[0]);
	}
	return FText(LOCTEXT("UK2Node_Blueprint_Template", "Node_Blueprint_Template Function"));
}

FText UK2Node_Blueprint_Template::GetMenuCategory() const
{
	if (ProxyClass && ProxyClass->HasMetaData(TEXT("Category")))
	{
		return FText::FromString(ProxyClass->GetMetaData(TEXT("Category")));
	}
	return Super::GetMenuCategory();
}

#if WITH_EDITORONLY_DATA
void UK2Node_Blueprint_Template::ResetToDefaultExposeOptions_Impl()
{
	if (ProxyClass)
	{
		if (const auto CDO = ProxyClass->GetDefaultObject<UBlueprintTaskTemplate>())
		{
			AllDelegates = CDO->AllDelegates;
			AllFunctions = CDO->AllFunctions;
			AllFunctionsExec = CDO->AllFunctionsExec;
			AllParam = CDO->AllParam;

			SpawnParam = CDO->SpawnParam;
			AutoCallFunction = CDO->AutoCallFunction;
			ExecFunction = CDO->ExecFunction;
			InDelegate = CDO->InDelegate;
			OutDelegate = CDO->OutDelegate;
		}
	}
	ReconstructNode();
}
#endif

void UK2Node_Blueprint_Template::CollectSpawnParam(UClass* InClass, const bool bFullRefresh)
{
	if (InClass)
	{
		if (const UBlueprintTaskTemplate* CDO = InClass->GetDefaultObject<UBlueprintTaskTemplate>())
		{
			AllDelegates = CDO->AllDelegates;
			AllFunctions = CDO->AllFunctions;
			AllFunctionsExec = CDO->AllFunctionsExec;
			AllParam = CDO->AllParam;
			if (bFullRefresh)
			{
				SpawnParam = CDO->SpawnParam;
				AutoCallFunction = CDO->AutoCallFunction;
				ExecFunction = CDO->ExecFunction;
				InDelegate = CDO->InDelegate;
				OutDelegate = CDO->OutDelegate;
			}
		}
	}
}
#undef LOCTEXT_NAMESPACE