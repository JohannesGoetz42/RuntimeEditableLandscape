// Fill out your copyright notice in the Description page of Project Settings.


#include "LayerTypes/LandscapeGroundTypeLayerData.h"

#include "LandscapeLayerComponent.h"
#include "RuntimeLandscape.h"
#include "Components/BoxComponent.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetRenderingLibrary.h"

void ULandscapeGroundTypeLayerData::ApplyToLandscape(ARuntimeLandscape* Landscape,
                                                     const ULandscapeLayerComponent* LandscapeLayerComponent) const
{
	if (ensure(GroundType->MaskRenderTarget))
	{
		const UBoxComponent* BoundingBox = LandscapeLayerComponent->GetOwner()->GetComponentByClass<UBoxComponent>();
		if (ensure(BoundingBox))
		{
			UCanvas* Canvas;
			FDrawToRenderTargetContext RenderTargetContext;
			FVector2D RenderTargetSize;
			UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(GetWorld(), GroundType->MaskRenderTarget, Canvas,
			                                                       RenderTargetSize,
			                                                       RenderTargetContext);

			const AActor* LayerOwner = LandscapeLayerComponent->GetOwner();
			const FVector2D LandscapeSize = Landscape->GetLandscapeSize();
			const FVector BoxExtent = BoundingBox->GetScaledBoxExtent();

			// calculate Position
			const FVector RelativePosition = LayerOwner->GetActorLocation() - Landscape->GetOriginLocation() - BoxExtent;
			const FVector2D Position = FVector2D(RelativePosition.X / LandscapeSize.X,
			                                     RelativePosition.Y / LandscapeSize.Y);
			const FVector2D ScreenPosition = FVector2D(Position.X * Canvas->SizeX, Position.Y * Canvas->SizeY);
			
			// calculate brush size
			const float AspectRatio = Canvas->SizeY / Canvas->SizeX;
			const float ScaleFactor = RenderTargetSize.X / LandscapeSize.X;
			const FVector BoxSize = BoxExtent * 2.0f;
			const FVector2D BrushSize = FVector2D(BoxSize.X, BoxSize.Y * AspectRatio) * ScaleFactor;

			const float Yaw = LayerOwner->GetActorRotation().Yaw;
			
			Canvas->K2_DrawMaterial(MaskBrushMaterial, ScreenPosition, BrushSize, FVector2D::Zero(),
			                        FVector2D::UnitVector, Yaw);
		}
	}
}
