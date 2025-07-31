// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LandscapeLayerDataBase.generated.h"

class URuntimeLandscapeComponent;
/**
 * Base class for landscape layers
 */
UCLASS(Abstract)
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeLayerDataBase : public UDataAsset
{
	GENERATED_BODY()

	friend class ULandscapeLayerComponent;
protected:	
	virtual void Apply(URuntimeLandscapeComponent* LandscapeComponent, const ULandscapeLayerComponent* LayerComponent, int32 VertexIndex, float& OutHeightValue, FColor& OutVertexColor, float SmoothingFactor) const
	{
		// this must be implemented in derived classes, otherwise the class has no use
		checkNoEntry();
	}
};
