// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeHole.h"
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

	void AddHole(TObjectPtr<const ALandscapeHole> Hole);
	void RemoveHole(TObjectPtr<const ALandscapeHole> Hole);
	void UpdateHoles();

protected:
	UPROPERTY()
	TSet<TObjectPtr<const ALandscapeHole>> Holes = TSet<TObjectPtr<const ALandscapeHole>>();
	UPROPERTY()
	TArray<float> InitialHeightValues = TArray<float>();
	UPROPERTY()
	/** All vertices that are inside at least one hole */
	TArray<int32> VerticesInHole = TArray<int32>();
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
};
