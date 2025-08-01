// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "LandscapeGroundTypeData.generated.h"

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
	TObjectPtr<UTextureRenderTarget2D> MaskRenderTarget;
	UPROPERTY(EditAnywhere)
	/** BE CAREFUL when increasing, since this can cause huge render target resolution */
	float PaintLayerResolution = 0.01f;
};
