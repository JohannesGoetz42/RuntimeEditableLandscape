// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeLandscape.generated.h"

class UProceduralMeshComponent;

UCLASS()
class RUNTIMEEDITABLELANDSCAPE_API ARuntimeLandscape : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARuntimeLandscape();

protected:
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1))
	FVector2D LandscapeSize = FVector2D(100, 100);
	UPROPERTY(EditAnywhere, meta = (ClampMin = 1))
	FVector2D MeshResolution = FVector2D(5, 5);
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProceduralMeshComponent> LandscapeMesh;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

#if WITH_EDITOR
	inline static const TSet<FString> MeshGenerationAffectingProperties = {"LandscapeSize", "MeshResolution"};
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	

	void GenerateMesh() const;
};
