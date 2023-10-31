//	Copyright 2020 Alexandr Marchenko. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_ExtendConstructObject.h"

#include "K2Node_Blueprint_Template.generated.h"

/** */
UCLASS()
class BLUEPRINTNODETEMPLATEEDITOR_API UK2Node_Blueprint_Template : public UK2Node_ExtendConstructObject
{
	GENERATED_BODY()
public:
	UK2Node_Blueprint_Template(const FObjectInitializer& ObjectInitializer);

	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override {}
	virtual void AllocateDefaultPins() override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;

#if WITH_EDITORONLY_DATA
	virtual void ResetToDefaultExposeOptions_Impl() override;
#endif

protected:
	void HideClassPin() const;
	void RegisterBlueprintAction(UClass* TargetClass, FBlueprintActionDatabaseRegistrar& ActionRegistrar) const;

	virtual void CollectSpawnParam(UClass* InClass, const bool bFullRefresh) override;
};
