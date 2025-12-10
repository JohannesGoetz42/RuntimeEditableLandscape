// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LandscapeLayerDataBase.generated.h"

class ARuntimeLandscape;
class URuntimeLandscapeComponent;

/**
 * Base class for landscape layers
 */
UCLASS(Abstract)
class RUNTIMEEDITABLELANDSCAPE_API ULandscapeLayerDataBase : public UDataAsset
{
	GENERATED_BODY()

	friend class ULandscapeLayerComponent;
	friend class ARuntimeLandscape;

protected:
	/** Override this for effects that apply their effect to the whole landscape */
	virtual void ApplyToLandscape(ARuntimeLandscape* Landscape,
	                              const ULandscapeLayerComponent* LandscapeLayerComponent) const
	{
	}

	/** Override this for effects that apply their effect based on vertices */
	virtual void ApplyToVertex(URuntimeLandscapeComponent* LandscapeComponent,
	                           const ULandscapeLayerComponent* LayerComponent, int32 VertexIndex,
	                           float& OutHeightValue, FColor& OutVertexColor, float SmoothingFactor) const
	{
	}

	/**
	 * Optional: Initialize a memory for this layer. Can be used to calculate stuff upfront instead of calculating for every vertex
	 */
	virtual void InitializeLayerMemory(
		const ULandscapeLayerComponent* OwningLayer)
	{
	}
};
