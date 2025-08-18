// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeGrassType.h"
#include "LandscapeLayerActor.h"
#include "ProceduralMeshComponent.h"
#include "RuntimeLandscapeComponent.generated.h"


struct FRuntimeLandscapeRebuildBuffer;
struct FLandscapeVertexData;
class UHierarchicalInstancedStaticMeshComponent;
class ARuntimeLandscape;
class ULandscapeLayerComponent;

UCLASS()
class RUNTIMEEDITABLELANDSCAPE_API URuntimeLandscapeComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

	friend class ARuntimeLandscape;
	friend class FGenerateVerticesRunner;
	friend class URuntimeLandscapeRebuildManager;

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

	void Initialize(int32 ComponentIndex, const TArray<float>& HeightValuesInitial);

	FORCEINLINE ARuntimeLandscape* GetParentLandscape() const { return ParentLandscape; }
	FORCEINLINE const TSet<TObjectPtr<const ULandscapeLayerComponent>>& GetAffectingLayers() const
	{
		return AffectingLayers;
	}

	FORCEINLINE int32 GetComponentIndex() const { return Index; }

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

	TArray<float> HeightValues = TArray<float>();

	UHierarchicalInstancedStaticMeshComponent* FindOrAddGrassMesh(const FGrassVariety& Variety);
	void Rebuild();
	void ApplyDataFromLayers(TArray<float>& OutHeightValues, TArray<FColor>& OutVertexColors);
	void UpdateNavigation();
	void RemoveFoliageAffectedByLayer() const;

	/** Applies data to the landscape after all threads are finished */
	void FinishRebuild(const FRuntimeLandscapeRebuildBuffer& RebuildBuffer);

private:
	bool bIsStale;
};
