// Copyright 2023 X-Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetHistoryTrackerPluginSettings.generated.h"


UCLASS(config=EditorPerProjectUserSettings, defaultconfig)
class ASSETHISTORYTRACKER_API UAssetHistoryTrackerPluginSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker", meta=(ClampMin=3, ClampMax=300))
	int Capacity = 150;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Blueprint")
	bool Blueprint = true;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Blueprint")
	bool WidgetBlueprint = true;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Blueprint")
	bool AnimBlueprint = false;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|UserDefined")
	bool UserDefinedEnum = true;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|UserDefined")
	bool UserDefinedStruct = true;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Material")
	bool Material = true;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Material")
	bool MaterialInstance = true;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Material")
	bool MaterialFunction = false;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Anim")
	bool AnimMontage = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Anim")
	bool AnimSequence = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Anim")
	bool AnimationAsset = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Sound")
	bool SoundWave = true;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Sound")
	bool SoundCue = true;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Sound")
	bool SoundAttenuation = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Sound")
	bool SoundMix = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Sound")
	bool SoundSubmix = false;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Curve")
	bool CurveTable = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Curve")
	bool CurveFloat = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Curve")
	bool CurveLinearColor = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Curve")
	bool CurveVector = false;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Skeleton")
	bool Skeleton = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Skeleton")
	bool SkeletalMesh = false;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|AI")
	bool BehaviorTree = true;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|AI")
	bool BlackboardData = true;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|AI")
	bool BlackboardComponent = false;
	
	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|AI")
	bool AISense = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Other")
	bool Font = false;

	UPROPERTY(EditAnywhere, config, Category="Asset History Tracker|Other")
	bool PhysicsAsset = true;
};
