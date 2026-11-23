// DriftDash - tyre trails.

#include "DriftSkidMarks.h"

UDriftSkidMarks::UDriftSkidMarks(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bUseComplexAsSimpleCollision = false;

	Sections.SetNum(SectionCount);
	const int32 Verts = QuadsPerSection * 4;
	// Both windings, so the marks show whichever way the quad was built.
	Triangles.Reserve(QuadsPerSection * 12);
	for (int32 Q = 0; Q < QuadsPerSection; ++Q)
	{
		const int32 B = Q * 4;
		Triangles.Append({ B + 0, B + 2, B + 1, B + 1, B + 2, B + 3, B + 0, B + 1, B + 2, B + 1, B + 3, B + 2 });
	}
	Normals.Init(FVector::UpVector, Verts);
	UV0.Init(FVector2D::ZeroVector, Verts);
	Colors.Init(FLinearColor::White, Verts);
	Tangents.Init(FProcMeshTangent(1.f, 0.f, 0.f), Verts);
}

void UDriftSkidMarks::SetMarkMaterial(UMaterialInterface* InMaterial)
{
	MarkMaterial = InMaterial;
	for (int32 i = 0; i < SectionCount; ++i)
	{
		if (Sections[i].bCreated)
		{
			SetMaterial(i, MarkMaterial);
		}
	}
}

void UDriftSkidMarks::AddPoint(int32 Trail, const FVector& Position, bool bActive)
{
	if (Trail < 0 || Trail >= NumTrails)
	{
		return;
	}
	FTrail& T = Trails[Trail];
	if (!bActive)
	{
		T.bHasPoint = false;
		T.bHasEdge = false;
		return;
	}
	if (!T.bHasPoint)
	{
		T.bHasPoint = true;
		T.bHasEdge = false;
		T.Point = Position;
		return;
	}
	FVector D = Position - T.Point;
	D.Z = 0.f;
	const float Dist = D.Size();
	if (Dist < MinSegment)
	{
		return;
	}
	if (Dist > MaxGap)
	{
		T.Point = Position;
		T.bHasEdge = false;
		return;
	}
	const FVector Side = FVector(-D.Y, D.X, 0.f) / Dist * (Width * 0.5f);
	const FVector A0 = T.bHasEdge ? T.EdgeL : T.Point - Side;
	const FVector A1 = T.bHasEdge ? T.EdgeR : T.Point + Side;
	const FVector B0 = Position - Side;
	const FVector B1 = Position + Side;
	AppendQuad(A0, A1, B0, B1);
	T.Point = Position;
	T.EdgeL = B0;
	T.EdgeR = B1;
	T.bHasEdge = true;
}

void UDriftSkidMarks::AppendQuad(const FVector& A0, const FVector& A1, const FVector& B0, const FVector& B1)
{
	FSection* S = &Sections[Current];
	if (S->Used >= QuadsPerSection)
	{
		Current = (Current + 1) % SectionCount;
		S = &Sections[Current];
		S->Used = 0;
	}
	if (S->Used == 0)
	{
		// Unused quads collapse onto the first point of the section (invisible, and the bounds stay tight).
		S->Vertices.Init(A0, QuadsPerSection * 4);
	}
	const int32 B = S->Used * 4;
	S->Vertices[B + 0] = A0;
	S->Vertices[B + 1] = A1;
	S->Vertices[B + 2] = B0;
	S->Vertices[B + 3] = B1;
	++S->Used;
	++TotalQuads;
	Upload(Current);
}

void UDriftSkidMarks::Upload(int32 SectionIndex)
{
	FSection& S = Sections[SectionIndex];
	if (!S.bCreated)
	{
		CreateMeshSection_LinearColor(SectionIndex, S.Vertices, Triangles, Normals, UV0, Colors, Tangents, false);
		SetMaterial(SectionIndex, MarkMaterial);
		S.bCreated = true;
	}
	else
	{
		UpdateMeshSection_LinearColor(SectionIndex, S.Vertices, Normals, UV0, Colors, Tangents);
	}
}

void UDriftSkidMarks::ClearMarks()
{
	ClearAllMeshSections();
	for (FSection& S : Sections)
	{
		S.Used = 0;
		S.bCreated = false;
		S.Vertices.Reset();
	}
	for (FTrail& T : Trails)
	{
		T = FTrail();
	}
	Current = 0;
	TotalQuads = 0;
}
