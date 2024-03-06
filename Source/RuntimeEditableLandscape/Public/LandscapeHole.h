// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandscapeHole.generated.h"

class URuntimeLandscapeComponent;
class ARuntimeLandscape;
class UCapsuleComponent;
class UBoxComponent;
class USphereComponent;

UENUM()
enum class EHoleShape : uint8
{
	HS_Box UMETA(DisplayName = "Box"),
	HS_Sphere UMETA(DisplayName = "Sphere")
};

UCLASS()
/**
 * Creates a hole in a RuntimeLandscape
 */
class RUNTIMEEDITABLELANDSCAPE_API ALandscapeHole : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeHole();
	bool IsLocationInside(const FVector& Location) const;
	FBox2D GetHoleBounds() const;

protected:
	UPROPERTY(EditAnywhere)
	TSet<TObjectPtr<ARuntimeLandscape>> AffectedLandscapes;
	UPROPERTY(EditAnywhere)
	EHoleShape Shape = EHoleShape::HS_Box;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Shape == EHoleShape::HS_Sphere", EditConditionHides))
	float Radius = 100.0f;
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Shape == EHoleShape::HS_Box", EditConditionHides))
	FVector Extent = FVector(100.0f);

	virtual void Destroyed() override;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<USphereComponent> SpherePreview;
	UPROPERTY()
	TObjectPtr<UBoxComponent> BoxPreview;
	UPROPERTY()
	TObjectPtr<UCapsuleComponent> CapsulePreview;

	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void UpdatePreviewComponents() const;
#endif
};
