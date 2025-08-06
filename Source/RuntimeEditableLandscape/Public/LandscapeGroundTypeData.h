// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "LandscapeGroundTypeData.generated.h"

class ULandscapeGrassType;
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
	TObjectPtr<ULandscapeGrassType> GrassType;
};
