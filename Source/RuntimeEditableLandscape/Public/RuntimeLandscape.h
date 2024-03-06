// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeLandscapeComponent.h"
#include "GameFramework/Actor.h"
#include "RuntimeLandscape.generated.h"

class URuntimeLandscapeComponent;
class ULandscapeLayerComponent;
class ALandscape;

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

	/** Get the amount of vertices in a single section */
	int32 GetVertexAmountPerSection() const
	{
		return (MeshResolution.X / ComponentAmount.X + 1) * (MeshResolution.Y / ComponentAmount.Y + 1);
	}

	/** Get the size of a single section */
	FORCEINLINE FVector2D GetSizePerComponent() const { return LandscapeSize / ComponentAmount; }
	FORCEINLINE const FVector2D& GetLandscapeSize() const { return LandscapeSize; }
	FORCEINLINE const FVector2D& GetMeshResolution() const { return MeshResolution; }
	FORCEINLINE const FVector2D& GetComponentAmount() const { return ComponentAmount; }

	/**
	 * Get the ids for the sections contained in the specified area
	 * Sections are numbered like this (i.e ComponentAmount = 4x4):
	 * 0	1	2	3	4
	 * 5	6	7	8	9
	 * 10	11	12	13	14
	 * 15	16	17	18	19
	 */
	TArray<int32> GetComponentsInArea(const FBox2D& Area) const;

	/**
	 * Get the grid coordinates of the specified section
	 * @param SectionId				The id of the section
	 * @param OutCoordinateResult	The coordinate result
	 */
	void GetComponentCoordinates(int32 SectionId, FIntVector2& OutCoordinateResult) const;

	FBox2D GetComponentBounds(int32 SectionIndex) const;

	/**
	 * Get the start location of the specified section relative to the actor location
	 * @param SectionIndex		The index of the section
	 */
	FVector2D GetRelativeSectionLocation(int32 SectionIndex) const
	{
		FIntVector2 SectionCoordinates;
		GetComponentCoordinates(SectionIndex, SectionCoordinates);
		return GetSizePerComponent() * FVector2D(SectionCoordinates.X, SectionCoordinates.Y);
	}

protected:
	UPROPERTY(EditAnywhere, Category = "Height data")
	TObjectPtr<ALandscape> LandscapeToCopyFrom;
	UPROPERTY()
	FVector2D LandscapeSize = FVector2D(1000, 1000);
	UPROPERTY()
	FVector2D MeshResolution = FVector2D(10, 10);
	//TODO: Ensure MeshResolution is a multiple of ComponentAmount
	UPROPERTY()
	FVector2D ComponentAmount = FVector2D(2.0f, 2.0f);
	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bUpdateCollision = true;
	UPROPERTY()
	TArray<TObjectPtr<URuntimeLandscapeComponent>> LandscapeComponents;
	UPROPERTY(EditAnywhere)
	float HeightScale = 1.0f;
	UPROPERTY(EditAnywhere)
	/** The side length of a single component in units (components are always squares) */
	float ComponentSize;

	UFUNCTION()
	void HandleLandscapeLayerOwnerDestroyed(AActor* DestroyedActor);

	void RemoveLandscapeLayer(ULandscapeLayerComponent* Layer, bool bForceRebuild = true)
	{
		for (URuntimeLandscapeComponent* LandscapeComponent : LandscapeComponents)
		{
			LandscapeComponent->RemoveLandscapeLayer(Layer, bForceRebuild);
		}
	}

	void InitializeHeightValues();
	bool ReadHeightValuesFromLandscape();
	void InitializeFromLandscape();

	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITORONLY_DATA

public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInterface> LandscapeMaterial;
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebug;
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebugCheckerBoard;
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (EditCondition="!bDrawDebugCheckerBoard", EditConditionHides))
	bool bDrawIndexGreyscales;
	UPROPERTY(EditAnywhere, Category = "Debug")
	FColor DebugColor1 = FColor::Blue;
	UPROPERTY(EditAnywhere, Category = "Debug")
	FColor DebugColor2 = FColor::Emerald;
	UPROPERTY(EditAnywhere, Category = "Debug")
	TObjectPtr<UMaterial> DebugMaterial;
	UPROPERTY(EditAnywhere, Category = "Debug")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

#endif
};
