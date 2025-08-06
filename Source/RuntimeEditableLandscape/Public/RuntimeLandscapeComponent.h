// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeGroundTypeData.h"
#include "LandscapeLayerActor.h"
#include "ProceduralMeshComponent.h"
#include "RuntimeLandscapeComponent.generated.h"


class UHierarchicalInstancedStaticMeshComponent;
class ARuntimeLandscape;
class ULandscapeLayerComponent;

UCLASS()
class RUNTIMEEDITABLELANDSCAPE_API URuntimeLandscapeComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

	friend class ARuntimeLandscape;

public:
	void AddLandscapeLayer(const ULandscapeLayerComponent* Layer);

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

	void RemoveLandscapeLayer(const ULandscapeLayerComponent* Layer)
	{
		AffectingLayers.Remove(Layer);
		Rebuild();
	}

	void Initialize(int32 ComponentIndex, const TArray<float>& HeightValues);

	FORCEINLINE const TSet<TObjectPtr<const ULandscapeLayerComponent>>& GetAffectingLayers() const
	{
		return AffectingLayers;
	}

	FVector2D GetRelativeVertexLocation(int32 VertexIndex) const;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

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
	TArray<UHierarchicalInstancedStaticMeshComponent*> GrassMeshes;

	void AddVertex(TArray<FVector>& OutVertices, const FVector& VertexLocation, int32 X, int32 Y);

	void UpdateGrassAtVertex(const ULandscapeGrassType* SelectedGrass, const FVector& VertexRelativeLocation,
	                         float Weight);
	void SetGrassForVertex(const FVector& VertexLocation, int32 X, int32 Y);
	void Rebuild();
	void ApplyDataFromLayers(TArray<float>& OutHeightValues, TArray<FColor>& OutVertexColors);
	void UpdateNavigation();
	void RemoveFoliageAffectedByLayer() const;

private:
	bool bIsStale;

	/**
	 * Does the actual rebuild after the lock.
	 * Should not be called. Call Rebuild() instead
	 */
	void ExecuteRebuild();
};
