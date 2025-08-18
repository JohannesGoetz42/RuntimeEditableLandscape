// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "LandscapeGroundTypeData.generated.h"

class ULandscapeGrassType;

USTRUCT()
struct FGrassTypeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<ULandscapeGrassType> GrassType;
	UPROPERTY(EditAnywhere)
	/**
	 * If the surface is steeper than this value, no grass of this type is applied
	 * Grass will always be spawned if left at zero
	 */
	float MaxSlopeAngle = 0.0f;
};

/**
 * Data asset that stores data for landscape ground types
 */
UCLASS()
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeGroundTypeData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FName LandscapeLayerName;
	UPROPERTY(EditAnywhere)
	FGrassTypeSettings GrassTypeSettings;
};
