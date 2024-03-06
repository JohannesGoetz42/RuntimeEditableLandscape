// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

protected:
	UPROPERTY()
	TArray<float> InitialHeightValues = TArray<float>();
	UPROPERTY()
	TSet<TObjectPtr<const ULandscapeLayerComponent>> AffectingLayers = TSet<TObjectPtr<const
		ULandscapeLayerComponent>>();
	UPROPERTY()
	const ARuntimeLandscape* ParentLandscape;

	int32 Index;
	bool bIsStale;

	void Rebuild(bool bUpdateCollision = true);
	void GenerateDataFromLayers(TArray<float>& OutHeightValues, TArray<FColor>& OutVertexColors);
};