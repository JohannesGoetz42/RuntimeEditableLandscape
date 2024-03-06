// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeLandscape.generated.h"

class ULandscapeLayerComponent;
class ALandscape;

USTRUCT()
struct FSectionData
{
	GENERATED_BODY()

	FSectionData(): SectionIndex(0)
	{
	}

	explicit FSectionData(int32 Section) : SectionIndex(Section)
	{
	}

	int32 SectionIndex;
	bool bIsStale = true;
	TSet<TObjectPtr<const ULandscapeLayerComponent>> AffectingLayers = TSet<TObjectPtr<const
		ULandscapeLayerComponent>>();
	TArray<float> HeightValues = TArray<float>();
};

class UProceduralMeshComponent;

UCLASS()
class RUNTIMEEDITABLELANDSCAPE_API ARuntimeLandscape : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARuntimeLandscape();

	/**
	 * Adds a new layer to the landscape
	 * @param LayerToAdd The added landscape layer
	 * @param bForceRebuild Whether the landscape should be updated immediately (see UpdateStaleSections()). This allows delaying the update for performance reasons
	 */
	void AddLandscapeLayer(const ULandscapeLayerComponent* LayerToAdd, bool bForceRebuild = true);

protected:
	UPROPERTY(EditAnywhere, Category = "Height data",
		meta = (EditCondition="LandscapeMode == ELandscapeMode::LM_CopyFromLandscape", EditConditionHides))
	TObjectPtr<ALandscape> LandscapeToCopyFrom;

	UPROPERTY(EditAnywhere, meta = (ClampMin = 1))
	FVector2D LandscapeSize = FVector2D(1000, 1000);
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1, FixedIncrement = 1))
	FVector2D MeshResolution = FVector2D(10, 10);
	//TODO: Ensure MeshResolution is a multiple of SectionAmount
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1, FixedIncrement = 1))
	FVector2D SectionAmount = FVector2D(2.0f, 2.0f);

	UPROPERTY(EditAnywhere)
	TObjectPtr<UProceduralMeshComponent> LandscapeMesh;
	UPROPERTY(EditAnywhere)
	TArray<FSectionData> Sections;

#if WITH_EDITORONLY_DATA	
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebug;
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugCheckerBoard;
	UPROPERTY(EditAnywhere, Category = "Debug")
	FColor DebugColor1 = FColor::Blue;
	UPROPERTY(EditAnywhere, Category = "Debug")
	FColor DebugColor2 = FColor::Emerald;
	UPROPERTY(EditAnywhere, Category = "Debug")
	TObjectPtr<UMaterial> DebugMaterial;
	UPROPERTY(EditAnywhere, Category = "Debug")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

#endif

	UFUNCTION()
	void HandleLandscapeLayerOwnerDestroyed(AActor* DestroyedActor);

	void RemoveLandscapeLayer(ULandscapeLayerComponent* Layer, bool bForceRebuild = true)
	{
		for (FSectionData Section : Sections)
		{
			Section.AffectingLayers.Remove(Layer);
			Section.bIsStale = true;
		}
		if (bForceRebuild)
		{
			GenerateMesh();
		}
	}

	void InitializeHeightValues();
	bool ReadHeightValuesFromLandscape();

	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/**
	 * Get the ids for the secions contained in the specified area
	 * Sections are numbered like this (i.e SectionAmount = 4x4):
	 * 0	1	2	3	4
	 * 5	6	7	8	9
	 * 10	11	12	13	14
	 * 15	16	17	18	19
	 */
	TArray<int32> GetSectionsInArea(const FBox2D& Area) const;

	/**
	 * Get the grid coordinates of the specified section
	 * @param SectionId				The id of the section
	 * @param OutCoordinateResult	The coordinate result
	 */
	void GetSectionCoordinates(int32 SectionId, FIntVector2& OutCoordinateResult) const;

	FBox2D GetSectionBounds(int32 SectionIndex) const;

	/** Get the amount of vertices in a single section */
	int32 GetVertexAmountPerSection() const
	{
		return GetVertexAmountPerSectionRow() * GetVertexAmountPerSectionColumn();
	}

	/** Get the amount of vertices in a single section row */
	FORCEINLINE int32 GetVertexAmountPerSectionRow() const { return MeshResolution.X / SectionAmount.X + 1; }
	/** Get the amount of vertices in a single section column */
	FORCEINLINE int32 GetVertexAmountPerSectionColumn() const { return MeshResolution.Y / SectionAmount.Y + 1; }

	/** Get the size of a single section */
	FORCEINLINE FVector2D GetSizePerSection() const { return LandscapeSize / SectionAmount; }

	void InitializeSections();
	//FBox2D GetLandscapeBounds() const;
	void GenerateDataFromLayers(FSectionData& Section, TArray<FColor>& OutVertexColors);
	void GenerateMesh();
	/**
	 * Generates the sections for the mesh
	 * @param bFullRebuild		Whether all sections should be generated or only stale ones
	 */
	void GenerateSections(bool bFullRebuild = false);
};
