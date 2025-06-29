// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeLayerActor.h"
#include "ProceduralMeshComponent.h"
#include "RuntimeLandscapeComponent.generated.h"


class ARuntimeLandscape;
class ULandscapeLayerComponent;

UCLASS()
class RUNTIMEEDITABLELANDSCAPE_API URuntimeLandscapeComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	void AddLandscapeLayer(const ULandscapeLayerComponent* Layer, bool bForceRebuild);

	void SetHoleFlagForVertex(int32 VertexIndex, bool bValue)
	{
		if (bValue)
		{
			VerticesInHole.Add(VertexIndex);
		}
		else
		{
			VerticesInHole.Remove(VertexIndex);
		}
	}

	void RemoveLandscapeLayer(const ULandscapeLayerComponent* Layer, bool bForceRebuild)
	{
		AffectingLayers.Remove(Layer);

		if (bForceRebuild)
		{
			Rebuild();
		}
		else
		{
			bIsStale = true;
		}
	}

	void Initialize(int32 ComponentIndex, const TArray<float>& HeightValues);

	FORCEINLINE const TSet<TObjectPtr<const ULandscapeLayerComponent>>& GetAffectingLayers() const
	{
		return AffectingLayers;
	}

	FVector2D GetRelativeVertexLocation(int32 VertexIndex) const;

protected:
	UPROPERTY()
	TArray<float> InitialHeightValues = TArray<float>();
	UPROPERTY()
	/** All vertices that are inside at least one hole */
	TSet<int32> VerticesInHole = TSet<int32>();
	UPROPERTY()
	TSet<TObjectPtr<const ULandscapeLayerComponent>> AffectingLayers = TSet<TObjectPtr<const
		ULandscapeLayerComponent>>();
	UPROPERTY()
	TObjectPtr<ARuntimeLandscape> ParentLandscape;
	UPROPERTY()
	int32 Index;
	UPROPERTY()
	bool bIsStale;

	void Rebuild();
	void ApplyDataFromLayers(TArray<float>& OutHeightValues, TArray<FColor>& OutVertexColors);
	void UpdateNavigation();
	void UpdateFoliage() const;
};
