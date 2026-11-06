// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "Editor/UnrealEdEngine.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersionComparison.h"
#include "MRUtilityKitGeometry.h"
#include "TestHelper.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"
#include "Editor.h"

namespace
{
	double CalculateTriangleArea(const FVector2D& P1, const FVector2D& P2, const FVector2D& P3)
	{
		return (P1.X * (P2.Y - P3.Y) + P2.X * (P3.Y - P1.Y) + P3.X * (P1.Y - P2.Y)) / 2.0f;
	}

	// Use the triangulated area as a proxy to ensure the triangulation worked as expected
	double CalculateTriangulatedArea(const TArray<FVector2D>& Vertices, const TArray<int32>& Indices)
	{
		double Area = 0.0;
		for (int i = 0; i < Indices.Num(); i += 3)
		{
			const FVector2D& P1 = Vertices[Indices[i]];
			const FVector2D& P2 = Vertices[Indices[i + 1]];
			const FVector2D& P3 = Vertices[Indices[i + 2]];
			const double TriangleArea = CalculateTriangleArea(P1, P2, P3);
			check(TriangleArea >= 0.0);
			Area += TriangleArea;
		}

		return Area;
	}
} // namespace

#if UE_VERSION_OLDER_THAN(5, 5, 0)
BEGIN_DEFINE_SPEC(FMRUKGeometrySpec, TEXT("MR Utility Kit"), EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
#else
BEGIN_DEFINE_SPEC(FMRUKGeometrySpec, TEXT("MR Utility Kit"), EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
#endif

void SetupMRUKSubsystem();
void TeardownMRUKSubsystem();
END_DEFINE_SPEC(FMRUKGeometrySpec)

void FMRUKGeometrySpec::SetupMRUKSubsystem()
{
	BeforeEach([this]() {
		// Load map and start play in editor
		const auto ContentDir = FPaths::ProjectContentDir();
		FAutomationEditorCommonUtils::LoadMap(ContentDir + "/Common/Maps/TestLevel.umap");
		StartPIE(true);
	});

	BeforeEach(EAsyncExecution::ThreadPool, []() {
		while (!GEditor->IsPlayingSessionInEditor())
		{
			// Wait until play session starts
			FGenericPlatformProcess::Yield();
		}
	});
}

void FMRUKGeometrySpec::TeardownMRUKSubsystem()
{
	// Caution: Order of these statements is important

	AfterEach(EAsyncExecution::ThreadPool, []() {
		while (GEditor->IsPlayingSessionInEditor())
		{
			// Wait until play session ends
			FGenericPlatformProcess::Yield();
		}
	});

	AfterEach([]() {
		// Request end of play session
		GUnrealEd->RequestEndPlayMap();
	});
}

void FMRUKGeometrySpec::Define()
{
	Describe(TEXT("Triangulation"), [this] {
		SetupMRUKSubsystem();

		It(TEXT("Triangulate quad"), [this] {
			const TArray<FVector2f> TestPolygon = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
			TArray<FVector2D> Vertices;
			TArray<int32> Indices;
			MRUKTriangulatePolygon({ TestPolygon }, Vertices, Indices);
			TestEqual(TEXT("Correct number of indices"), 6, Indices.Num());
			TestEqual(TEXT("Correct area triangulated"), 1.0, CalculateTriangulatedArea(Vertices, Indices));
		});

		It(TEXT("Triangulate quad with hole"), [this] {
			const TArray<TArray<FVector2f>> Polygons = { { { 0.0f, 0.0f }, { 2.0f, 0.0f }, { 2.0f, 2.0f }, { 0.0f, 2.0f } }, { { 0.5f, 0.5f }, { 0.5f, 1.5f }, { 1.5f, 1.5f }, { 1.5f, 0.5f } } };
			TArray<FVector2D> Vertices;
			TArray<int32> Indices;
			MRUKTriangulatePolygon(Polygons, Vertices, Indices);

			TestEqual(TEXT("Correct number of indices"), Indices.Num(), 24);
			TestEqual(TEXT("Correct area triangulated"), CalculateTriangulatedArea(Vertices, Indices), 3.0);
		});

		It(TEXT("Triangulate quad with four holes"), [this] {
			TArray<TArray<FVector2f>> Polygons = { { { 0.0f, 0.0f }, { 4.0f, 0.0f }, { 4.0f, 4.0f }, { 0.0f, 4.0f } } };
			for (int32 I = 0; I < 4; ++I)
			{
				const FVector2f Offset(0.5f + 2.0f * (I / 2), 0.5f + 2.0f * (I % 2));
				Polygons.Push({ Offset + FVector2f(0.0f, 0.0f), Offset + FVector2f(0.0f, 1.0f), Offset + FVector2f(1.0f, 1.0f), Offset + FVector2f(1.0f, 0.0f) });
			}

			TArray<FVector2D> Vertices;
			TArray<int32> Indices;
			MRUKTriangulatePolygon(Polygons, Vertices, Indices);

			TestEqual(TEXT("Correct number of indices"), Indices.Num(), 66);
			TestEqual(TEXT("Correct area triangulated"), CalculateTriangulatedArea(Vertices, Indices), 12.0);
		});

		It(TEXT("Triangulate quad with two close holes"), [this] {
			const TArray<TArray<FVector2f>> Polygons = {
				{
					{ 101.985214f, 113.8258f },
					{ -101.985214f, 113.8258f },
					{ -101.985214f, -113.8258f },
					{ 101.985214f, -113.8258f },
				},
				{ { 18.395055731633885f, 9.0596833f }, { -72.518264268366110f, 9.0596833f }, { -72.518264268366110f, 67.2252527f }, { 18.395055731633885f, 67.2252527f } },
				{ { 18.395055731633885f, -53.4203167f }, { -72.518264268366110f, -53.4203167f }, { -72.518264268366110f, 4.7452569f }, { 18.395055731633885f, 4.7452569f } },
			};

			TArray<FVector2D> Vertices;
			TArray<int32> Indices;
			MRUKTriangulatePolygon(Polygons, Vertices, Indices);

			TestEqual(TEXT("Correct number of indices"), Indices.Num(), 42);
			TestEqual(TEXT("Correct area triangulated"), CalculateTriangulatedArea(Vertices, Indices), 35858.143857, 0.001);
		});

		It(TEXT("Triangulate LShape"), [this] {
			const TArray<FVector2f> TestPolygon = { { 0.0f, 0.0f }, { 2.0f, 0.0f }, { 2.0f, 2.0f }, { 1.0f, 2.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
			TArray<FVector2D> Vertices;
			TArray<int32> Indices;
			MRUKTriangulatePolygon({ TestPolygon }, Vertices, Indices);

			TestEqual(TEXT("Correct number of indices"), Indices.Num(), 12);
			TestEqual(TEXT("Correct area triangulated"), CalculateTriangulatedArea(Vertices, Indices), 3.0);
		});

		It(TEXT("Triangulate CShape"), [this] {
			const TArray<FVector2f> TestPolygon = { { 0.0f, 0.0f }, { 2.0f, 0.0f }, { 2.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f }, { 2.0f, 3.0f }, { 0.0f, 3.0f } };
			TArray<FVector2D> Vertices;
			TArray<int32> Indices;
			MRUKTriangulatePolygon({ TestPolygon }, Vertices, Indices);

			TestEqual(TEXT("Correct number of indices"), Indices.Num(), 18);
			TestEqual(TEXT("Correct area triangulated"), CalculateTriangulatedArea(Vertices, Indices), 5.0);
		});

		TeardownMRUKSubsystem();
	});
}
