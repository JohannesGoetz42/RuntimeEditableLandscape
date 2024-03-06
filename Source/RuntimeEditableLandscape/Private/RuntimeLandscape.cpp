// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeLandscape.h"

#include "Landscape.h"
#include "LandscapeLayerComponent.h"
#include "ProceduralMeshComponent.h"

ARuntimeLandscape::ARuntimeLandscape()
{
	LandscapeMesh = CreateDefaultSubobject<UProceduralMeshComponent>("Landscape mesh");
	RootComponent = LandscapeMesh;
}

void ARuntimeLandscape::AddLandscapeLayer(const ULandscapeLayerComponent* LayerToAdd, bool bForceRebuild)
{
	if (!ensure(LayerToAdd))
	{
		return;
	}

	for (const int32 AffectedSectionIndex : GetSectionsInArea(LayerToAdd->GetAffectedArea()))
	{
		FSectionData& AffectedSection = Sections[AffectedSectionIndex];
		AffectedSection.AffectingLayers.Add(LayerToAdd);
		AffectedSection.bIsStale = true;
	}

	if (bForceRebuild)
	{
		GenerateSections();
	}
}

void ARuntimeLandscape::BeginPlay()
{
	Super::BeginPlay();
	GenerateMesh();

	if (LandscapeToCopyFrom)
	{
		LandscapeToCopyFrom->Destroy();
	}
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef - bound to delegate
void ARuntimeLandscape::HandleLandscapeLayerOwnerDestroyed(AActor* DestroyedActor)
{
	TArray<ULandscapeLayerComponent*> LandscapeLayers;
	DestroyedActor->GetComponents(LandscapeLayers);

	for (ULandscapeLayerComponent* Layer : LandscapeLayers)
	{
		RemoveLandscapeLayer(Layer);
	}
}

void ARuntimeLandscape::InitializeHeightValues()
{
	if (!ReadHeightValuesFromLandscape())
	{
		const int32 VertexAmountPerSection = GetVertexAmountPerSection();
		for (FSectionData& Section : Sections)
		{
			Section.HeightValues.Init(0, VertexAmountPerSection - Section.HeightValues.Num());
		}
	}
}

bool ARuntimeLandscape::ReadHeightValuesFromLandscape()
{
	TArray<float> LandscapeHeightData;
	int32 SizeX;
	int32 SizeY;
	LandscapeToCopyFrom->GetHeightValues(SizeX, SizeY, LandscapeHeightData);
	//
	const FVector2D SizePerSection = GetSizePerSection();
	const FBox ParentLandscapeBounds = LandscapeToCopyFrom->GetCompleteBounds();
	const FVector2D ParentLandscapeSize = FVector2D(ParentLandscapeBounds.Max.X - ParentLandscapeBounds.Min.X,
	                                                ParentLandscapeBounds.Max.Y - ParentLandscapeBounds.Min.Y);
	UE_LOG(LogTemp, Display, TEXT("LandscapeSize: %s"), *ParentLandscapeSize.ToString());
	// const FVector2D ParentToLocalOffset = FVector2D(GetActorLocation() - ParentLandscapeBounds.Min);
	// if (ParentToLocalOffset.X < 0 || ParentToLocalOffset.Y < 0)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("Runtime landscape '%s' is outside of it's parents bounds!"), *GetName());
	// 	return false;
	// }

	// const FVector2D WorldUnitsPerParentVertex = FVector2D(ParentLandscapeSize.X / SizeX, ParentLandscapeSize.Y / SizeY);
	const FIntVector2 SampleInterval = FIntVector2(
		SizeX / MeshResolution.X * (LandscapeSize.X / ParentLandscapeSize.X),
		SizeY / MeshResolution.Y * (LandscapeSize.Y / ParentLandscapeSize.Y));
	// const FVector2D SampleIntervalFloat = FVector2D(
	// 	SizeX / MeshResolution.X * (LandscapeSize.X / ParentLandscapeSize.X),
	// 	SizeY / MeshResolution.Y * (LandscapeSize.Y / ParentLandscapeSize.Y));

	// compensate sample error to avoid scaling issues
	// FVector2D SampleError = FVector2D(
	// 	SizeX / MeshResolution.X * (LandscapeSize.X / ParentLandscapeSize.X),
	// 	SizeY / MeshResolution.Y * (LandscapeSize.Y / ParentLandscapeSize.Y));
	// SampleError.X -= SampleInterval.X;
	// SampleError.Y -= SampleInterval.Y;

	// UE_LOG(LogTemp, Display, TEXT("#"));
	// UE_LOG(LogTemp, Display, TEXT("#"));
	// UE_LOG(LogTemp, Display, TEXT("IntMAX: %i"), INT32_MAX);
	for (FSectionData& Section : Sections)
	{
		FIntVector2 SectionCoordinates;
		GetSectionCoordinates(Section.SectionIndex, SectionCoordinates);

		float WorldOffsetX = GetActorLocation().X - LandscapeToCopyFrom->GetActorLocation().X + SectionCoordinates.X
			*
			SizePerSection.X;
		float WorldOffsetY = GetActorLocation().Y - LandscapeToCopyFrom->GetActorLocation().Y + SectionCoordinates.Y
			*
			SizePerSection.Y;

		int32 SectionStartOffsetX = SizeX / ParentLandscapeSize.X * WorldOffsetX;
		int32 SectionStartOffsetY = SizeY / ParentLandscapeSize.Y * WorldOffsetY;
		// calculate the section start offset in the height data
		// const FVector2D SectionRelativeOffset = SizePerSection * FVector2D(SectionCoordinates.X, SectionCoordinates.Y);
		// const int32 SectionStartOffsetX = FMath::FloorToInt(
		// 	(ParentToLocalOffset.X + SectionRelativeOffset.X) / WorldUnitsPerParentVertex.X);
		// const int32 SectionStartOffsetY = FMath::FloorToInt(
		// 	(ParentToLocalOffset.Y + SectionRelativeOffset.Y) / WorldUnitsPerParentVertex.Y * SizeX);

		FVector2D CurrentSampleError;
		// const float TEST = (ParentToLocalOffset.Y + SectionRelativeOffset.Y) / WorldUnitsPerParentVertex.Y;
		// UE_LOG(LogTemp, Display, TEXT("Section: %i"), SectionData.SectionIndex);
		// UE_LOG(LogTemp, Display, TEXT("		StartOffsetX: %i"), SectionStartOffsetX);
		// UE_LOG(LogTemp, Display, TEXT("		StartOffsetY: %i"), SectionStartOffsetY);
		UE_LOG(LogTemp, Display, TEXT("		SampleIntX: %i"), SampleInterval.X);
		UE_LOG(LogTemp, Display, TEXT("		SampleIntY: %i"), SampleInterval.Y);
		// UE_LOG(LogTemp, Display, TEXT("		SampleIntX: %f"), SampleIntervalFloat.X);
		// UE_LOG(LogTemp, Display, TEXT("		SampleIntY: %f"), SampleIntervalFloat.Y);
		// UE_LOG(LogTemp, Display, TEXT("		SampleErrorX: %f"), SampleError.X);
		// UE_LOG(LogTemp, Display, TEXT("		SampleErrorY: %f"), SampleError.Y);
		// UE_LOG(LogTemp, Display, TEXT("		TEST: %f"), TEST);

		int32 SampleCorrectionCountX = 0;
		for (int32 Y = 0; Y < GetVertexAmountPerSectionRow(); Y++)
		{
			for (int32 X = 0; X < GetVertexAmountPerSectionColumn(); X++)
			{
				// TODO: Remove if statement after finishing implementation
				// int32 VertexIndex = SectionStartOffsetX + SectionStartOffsetY + X * (SampleInterval.X + SampleError.X) + Y *
				// 	SizeX * (SampleInterval.Y + SampleError.Y);
				// int32 VertexIndexFloat = SectionStartOffsetX + SectionStartOffsetY + X * SampleIntervalFloat.X + Y *
				// 	SizeX * SampleIntervalFloat.Y;
				//
				// int32 VertexIndex = SectionStartOffsetX + SectionStartOffsetY + X * SampleInterval.X + Y *
				// 	SizeX * SampleInterval.Y;
				// const int32 VertexIndex = (SectionStartOffsetX + SectionStartOffsetY) + X * SampleInterval.X + Y *
				// 	SampleInterval.X * SizeX;
				const int32 VertexIndex = SectionStartOffsetY * SizeX + SectionStartOffsetX + X * SampleInterval.X +
					SizeX * SampleInterval.Y * Y;
				//UE_LOG(LogTemp, Display, TEXT("IndexDiff: %i, float: %i diff: %i"), VertexIndex, VertexIndexFloat, VertexIndexFloat - VertexIndex);
				//UE_LOG(LogTemp, Display, TEXT("		at: x %i, y %i section %i"), X, Y, SectionData.SectionIndex);
				if (LandscapeHeightData.IsValidIndex(VertexIndex))
				{
					// SectionData.HeightValues.Add(
					// LandscapeHeightData[SampleInterval.X * X + OffsetY + SectionStartOffsetX]);

					// compensate sample error
					// CurrentSampleError.X += SampleError.X;
					// if (CurrentSampleError.X > 1.0f)
					// {
					// 	VertexIndex++;
					// 	CurrentSampleError.X--;
					// 	SampleCorrectionCountX++;
					// 	UE_LOG(LogTemp, Display, TEXT("		Compensated sample error: %i at %i, %i"), SampleCorrectionCountX, X, Y);
					// }

					Section.HeightValues.Add(
						LandscapeHeightData[VertexIndex]);
				}
				else
				{
					Section.HeightValues.Add(0.0f);
					UE_LOG(LogTemp, Warning, TEXT("InvalidIndex %i!"), VertexIndex);
					UE_LOG(LogTemp, Warning, TEXT("X: %i, Y %i, SampleX: %i, SampleY: %i"), X, X, SampleInterval.X,
					       SampleInterval.Y);
					UE_LOG(LogTemp, Warning, TEXT("  StartOffsetX: %i StartOffsetY: %i"), SectionStartOffsetX,
					       SectionStartOffsetY);
					UE_LOG(LogTemp, Warning, TEXT("  MaxIndex: %i"), LandscapeHeightData.Num() - 1);
				}
			}

			// reset the sample error correction to ensure it is at the same column for each row
			CurrentSampleError.X = 0.0f;
		}
	}

	return true;
}

TArray<int32> ARuntimeLandscape::GetSectionsInArea(const FBox2D& Area) const
{
	// check if the area is inside the landscape
	if (Area.Min.X > GetActorLocation().X + LandscapeSize.X || Area.Min.Y > GetActorLocation().Y + LandscapeSize.Y ||
		Area.Max.X < GetActorLocation().X || Area.Max.Y < GetActorLocation().Y)
	{
		return TArray<int32>();
	}

	const FVector2D SectionSize = GetSizePerSection();

	const int32 MinColumn = FMath::Max(FMath::FloorToInt(Area.Min.X / SectionSize.X), 0);
	const int32 MaxColumn = FMath::Min(FMath::FloorToInt(Area.Max.X / SectionSize.X), SectionAmount.X - 1);

	const int32 MinRow = FMath::Max(FMath::FloorToInt(Area.Min.Y / SectionSize.Y), 0);
	const int32 MaxRow = FMath::Min(FMath::FloorToInt(Area.Max.Y / SectionSize.Y), SectionAmount.Y - 1);

	TArray<int32> Result;
	const int32 ExpectedAmount = (MaxColumn - MinColumn + 1) * (MaxRow - MinRow + 1);
	Result.Reserve(ExpectedAmount);

	for (int32 Y = MinRow; Y <= MaxRow; Y++)
	{
		const int32 RowOffset = Y * SectionAmount.X;
		for (int32 X = MinColumn; X <= MaxColumn; X++)
		{
			Result.Add(RowOffset + X);
		}
	}

	check(Result.Num() == ExpectedAmount);
	return Result;
}

void ARuntimeLandscape::GetSectionCoordinates(int32 SectionId, FIntVector2& OutCoordinateResult) const
{
	OutCoordinateResult.X = SectionId % FMath::RoundToInt(SectionAmount.X);
	OutCoordinateResult.Y = FMath::FloorToInt(SectionId / SectionAmount.X);
}

FBox2D ARuntimeLandscape::GetSectionBounds(int32 SectionIndex) const
{
	const FVector2D SectionSize = LandscapeSize / SectionAmount;
	FIntVector2 SectionCoordinates;
	GetSectionCoordinates(SectionIndex, SectionCoordinates);

	return FBox2D(
		FVector2D(SectionCoordinates.X * SectionSize.X, SectionCoordinates.Y * SectionSize.Y),
		FVector2D((SectionCoordinates.X + 1) * SectionSize.X, (SectionCoordinates.Y + 1) * SectionSize.Y));
}

//
// FBox2D ARuntimeLandscape::GetLandscapeBounds() const
// {
// 	const FVector2D LandscapeHorizontalLocation = FVector2D(GetActorLocation());
// 	return FBox2D(FVector2D(GetActorLocation()), LandscapeHorizontalLocation + LandscapeSize);
// }

void ARuntimeLandscape::InitializeSections()
{
	// remember existing layers
	TSet<TObjectPtr<const ULandscapeLayerComponent>> LandscapeLayers;
	for (const FSectionData& Section : Sections)
	{
		LandscapeLayers.Append(Section.AffectingLayers.Array());
	}

	Sections.Empty();

	const int32 RequiredSections = SectionAmount.X * SectionAmount.Y;
	Sections.Reserve(RequiredSections);
	for (int32 i = 0; i < RequiredSections; i++)
	{
		Sections.Add(FSectionData(i));
	}

	InitializeHeightValues();

	for (TObjectPtr<const ULandscapeLayerComponent> Layer : LandscapeLayers)
	{
		AddLandscapeLayer(Layer, false);
	}
}

void ARuntimeLandscape::GenerateDataFromLayers(FSectionData& Section, TArray<float>& OutHeightValues,
                                               TArray<FColor>& OutVertexColors)
{
	check(OutHeightValues.Num() == Section.HeightValues.Num());

	OutVertexColors.Init(FColor::White, Section.HeightValues.Num());
	for (const ULandscapeLayerComponent* Layer : Section.AffectingLayers)
	{
		for (int32 i = 0; i < Section.HeightValues.Num(); i++)
		{
			// check if the vertex is inside layer bounds			
			if (Layer->GetAffectedArea().IsInside(GetVertexLocation(Section.SectionIndex, i)))
			{
				if (Layer->LayerData.bApplyHeight)
				{
					OutHeightValues[i] = Layer->LayerData.HeightValue;
				}

				if (Layer->LayerData.bApplyVertexColor)
				{
					OutVertexColors[i] = Layer->LayerData.VertexColor;
				}
			}
		}
	}
}

void ARuntimeLandscape::GenerateMesh()
{
	LandscapeMesh->ClearAllMeshSections();
	InitializeSections();
	GenerateSections();
}

void ARuntimeLandscape::GenerateSections(bool bFullRebuild)
{
	const int32 VertexAmountPerSection = GetVertexAmountPerSection();
	const FIntVector2 SectionResolution = FIntVector2(FMath::FloorToInt(MeshResolution.X / SectionAmount.X),
	                                                  FMath::FloorToInt(MeshResolution.Y / SectionAmount.Y));

	for (FSectionData& Section : Sections)
	{
		if (!(bFullRebuild || Section.bIsStale))
		{
			continue;
		}

		// ensure the section data is valid
		if (!ensure(Section.HeightValues.Num() == VertexAmountPerSection))
		{
			UE_LOG(LogTemp, Warning, TEXT("Section %i could not generate valid data and will not be generated!"),
			       Section.SectionIndex);
			continue;
		}

		const TArray<FVector> Normals;
		const TArray<FVector2D> UV0;
		const TArray<FProcMeshTangent> Tangents;
		TArray<float> HeightValues = Section.HeightValues;
		TArray<FColor> VertexColors;

		GenerateDataFromLayers(Section, HeightValues, VertexColors);

		const FVector2D SectionOffset = GetSectionBounds(Section.SectionIndex).Min;
		TArray<FVector> Vertices;
		Vertices.Reserve(VertexAmountPerSection);
		TArray<int32> Triangles;
		Triangles.Reserve(SectionResolution.X * SectionResolution.Y * 2);

		const FVector2D VertexDistance = LandscapeSize / MeshResolution;
		int32 VertexIndex = 0;
		// generate first row of points
		for (int32 X = 0; X <= SectionResolution.X; X++)
		{
			Vertices.Add(FVector(X * VertexDistance.X + SectionOffset.X, SectionOffset.Y,
			                     HeightValues[VertexIndex]));
			VertexIndex++;
		}

		for (int32 Y = 0; Y < SectionResolution.Y; Y++)
		{
			const float Y1 = Y + 1;
			Vertices.Add(FVector(SectionOffset.X, Y1 * VertexDistance.Y + SectionOffset.Y,
			                     HeightValues[VertexIndex]));
			VertexIndex++;

			// generate triangle strip in X direction
			for (int32 X = 0; X < SectionResolution.X; X++)
			{
				Vertices.Add(FVector((X + 1) * VertexDistance.X + SectionOffset.X,
				                     Y1 * VertexDistance.Y + SectionOffset.Y,
				                     HeightValues[VertexIndex]));
				VertexIndex++;

				int32 T1 = Y * (SectionResolution.X + 1) + X;
				int32 T2 = T1 + SectionResolution.X + 1;
				int32 T3 = T1 + 1;

				// add upper-left triangle
				Triangles.Add(T1);
				Triangles.Add(T2);
				Triangles.Add(T3);

				// add lower-right triangle
				Triangles.Add(T3);
				Triangles.Add(T2);
				Triangles.Add(T2 + 1);
			}
		}

		// if The vertex amount is differs, something is wrong with the algorithm
		check(Vertices.Num() == VertexAmountPerSection);
		// if The triangle amount is differs, something is wrong with the algorithm
		check(Triangles.Num() == SectionResolution.X * SectionResolution.Y * 6);

#if WITH_EDITORONLY_DATA
		if (bEnableDebug && DebugMaterial)
		{
			LandscapeMesh->CleanUpOverrideMaterials();
			LandscapeMesh->SetMaterial(Section.SectionIndex, DebugMaterial);

			if (bDrawDebugCheckerBoard)
			{
				FIntVector2 SectionCoordinates;
				GetSectionCoordinates(Section.SectionIndex, SectionCoordinates);
				const bool bIsEvenRow = SectionCoordinates.Y % 2 == 0;
				const bool bIsEvenColumn = SectionCoordinates.X % 2 == 0;
				const FColor SectionColor = bIsEvenColumn && bIsEvenRow || !bIsEvenColumn && !bIsEvenRow
					                            ? DebugColor1
					                            : DebugColor2;
				VertexColors.Init(SectionColor, Vertices.Num());
			}
		}
#endif

		LandscapeMesh->CreateMeshSection(Section.SectionIndex, Vertices, Triangles, Normals, UV0, VertexColors,
		                                 Tangents,
		                                 false);
		Section.bIsStale = false;
	}
}

#if WITH_EDITOR
void ARuntimeLandscape::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MeshResolution.X = FMath::RoundToInt(MeshResolution.X);
	MeshResolution.Y = FMath::RoundToInt(MeshResolution.Y);

	GenerateMesh();
}
#endif
