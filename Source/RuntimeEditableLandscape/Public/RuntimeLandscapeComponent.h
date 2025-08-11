// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeGrassType.h"
#include "LandscapeLayerActor.h"
#include "ProceduralMeshComponent.h"
#include "Threads/GenerateVertexRowDataThread.h"
#include "RuntimeLandscapeComponent.generated.h"


struct FLandscapeVertexData;
class UHierarchicalInstancedStaticMeshComponent;
class ARuntimeLandscape;
class ULandscapeLayerComponent;

UCLASS()
class RUNTIMEEDITABLELANDSCAPE_API URuntimeLandscapeComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

	friend class ARuntimeLandscape;
	friend class FGenerateVertexRowDataThread;

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
	// TODO: Don't store this here!
	FLandscapeVertexData FirstRowVertexData;
	TArray<float> HeightValues = TArray<float>();
	TArray<FGenerateVertexRowDataThread*> GenerationThreads;
	int32 ActiveGenerationThreads;

	UHierarchicalInstancedStaticMeshComponent* FindOrAddGrassMesh(const FGrassVariety& Variety);
	void GenerateVertexData(FLandscapeVertexData& OutVertexData, const FVector& VertexLocation, int32 X, int32 Y) const;
	void GenerateGrassTransformsAtVertex(FLandscapeVertexData& OutVertexData, const ULandscapeGrassType* SelectedGrass,
	                                     const FVector& VertexRelativeLocation, float Weight) const;
	void GenerateGrassDataForVertex(FLandscapeVertexData& OutVertexData, const FVector& VertexLocation, int32 X,
	                                int32 Y) const;
	void Rebuild();
	void ApplyDataFromLayers(TArray<float>& OutHeightValues, TArray<FColor>& OutVertexColors);
	void UpdateNavigation();
	void RemoveFoliageAffectedByLayer() const;

	void GetRandomGrassRotation(const FGrassVariety& Variety, FRotator& OutRotation) const;
	void GetRandomGrassLocation(const FVector& VertexRelativeLocation, FVector& OutGrassLocation) const;
	void GetRandomGrassScale(const FGrassVariety& Variety, FVector& OutScale) const;
	void CheckThreadsFinished();
	/** Applies data to the landscape after all threads are finished */
	void FinishRebuild();

private:
	bool bIsStale;

	/**
	 * Does the actual rebuild after the lock.
	 * Should not be called. Call Rebuild() instead
	 */
	void ExecuteRebuild();
};
