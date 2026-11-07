/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Licensed under the Oculus SDK License Agreement (the "License");
 * you may not use the Oculus SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.
 *
 * You may obtain a copy of the License at
 *
 * https://developer.oculus.com/licenses/oculussdk/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "InteractorVisuals/IsdkRayVisualComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Curves/CurveFloat.h"
#include "UObject/ConstructorHelpers.h"
#include "IsdkContentAssetPaths.h"
#include "IsdkFunctionLibrary.h"
#include "InteractorVisuals/IsdkRayVisualFunctionLibrary.h"

// Sets default values for this component's properties
UIsdkRayVisualComponent::UIsdkRayVisualComponent()
{
  PrimaryComponentTick.bCanEverTick = false;

  PinchArrow = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Pinch Arrow"));
  // Defer attachment of component's subcomponents to begin play to prevent inheritance
  // template ensures
  PinchArrow->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
  PinchArrow->SetRelativeScale3D(FVector(0.01, 0.01, 0.01));
  static ConstructorHelpers::FObjectFinder<USkeletalMesh> PinchArrowMeshFinder(
      IsdkContentAssetPaths::Models::RayVisual::OculusHandPinchArrowBlended);
  if (ensureMsgf(PinchArrowMeshFinder.Succeeded(), TEXT("Could not load Pinch Arrow's Mesh.")))
  {
    SetPinchArrowMesh(PinchArrowMeshFinder.Object);
  }

  static ConstructorHelpers::FObjectFinder<UMaterial> PinchArrowMaterialFinder(
      IsdkContentAssetPaths::Materials::PinchArrow);
  if (ensureMsgf(
          PinchArrowMaterialFinder.Succeeded(), TEXT("Could not load Pinch Arrow's Material.")))
  {
    SetPinchArrowMaterial(PinchArrowMaterialFinder.Object);
  }
  PinchArrowColor = FLinearColor(1.0, 1.0, 1.0);

  Cursor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cursor Plane"));
  // Defer attachment of component's subcomponents to begin play to prevent inheritance
  // template ensures
  Cursor->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
  Cursor->SetRelativeScale3D(FVector(0.06, 0.06, 0.06));
  static ConstructorHelpers::FObjectFinder<UStaticMesh> CursorMeshFinder(
      IsdkContentAssetPaths::Engine::BasicShapes::Plane);
  if (ensureMsgf(CursorMeshFinder.Succeeded(), TEXT("Could not load Engine's Plane Basic Shape.")))
  {
    SetCursorMesh(CursorMeshFinder.Object);
  }
  TranslucentSortPriority = 1000;

  static ConstructorHelpers::FObjectFinder<UMaterial> CursorMaterialFinder(
      IsdkContentAssetPaths::Materials::Cursor);
  if (ensureMsgf(CursorMaterialFinder.Succeeded(), TEXT("Could not load Cursor's Material.")))
  {
    SetCursorMaterial(CursorMaterialFinder.Object);
  }
  CursorAccentColor = FLinearColor(0.031f, 0.376f, 0.965f);
  CursorColor = FLinearColor(1.0, 1.0, 1.0);
  CursorShadowColor = FLinearColor(0.0, 0.0, 0.0);

  static ConstructorHelpers::FObjectFinder<UCurveFloat> PinchStrengthCurve(
      IsdkContentAssetPaths::Models::RayVisual::PinchStrengthCurve);
  if (ensureMsgf(PinchStrengthCurve.Succeeded(), TEXT("Could not load Pinch's Strength Curve.")))
  {
    PinchArrowStrengthCurve = PinchStrengthCurve.Object;
  }

  MorphTargetNames.Empty();
  MorphTargetNames.Reserve(2);
  MorphTargetNames.Emplace(TEXT("ClosingCone"));
  MorphTargetNames.Emplace(TEXT("PopCone"));

  ReferenceDistance = 50.0;
  CursorSizeMinMax = FVector2D(0.02, 0.015);
}

void UIsdkRayVisualComponent::BeginPlay()
{
  Super::BeginPlay();
  const FAttachmentTransformRules Rules(
      EAttachmentRule::KeepRelative,
      EAttachmentRule::KeepRelative,
      EAttachmentRule::KeepRelative,
      /*bWeldSimulatedBodies*/ false);
  PinchArrow->AttachToComponent(this, Rules);
  Cursor->AttachToComponent(this, Rules);
  Initialize();
}

void UIsdkRayVisualComponent::OnRegister()
{
  Super::OnRegister();
  Initialize();
}

void UIsdkRayVisualComponent::OnVisibilityChanged()
{
  Super::OnVisibilityChanged();
  if (IsValid(PinchArrow))
  {
    PinchArrow->SetVisibility(IsVisible(), true);
  }
  if (IsValid(Cursor))
  {
    Cursor->SetVisibility(IsVisible(), true);
  }
}

void UIsdkRayVisualComponent::DestroyComponent(bool bPromoteChildren /*= false*/)
{
  if (IsValid(PinchArrow))
  {
    PinchArrow->DestroyComponent(bPromoteChildren);
  }
  if (IsValid(Cursor))
  {
    Cursor->DestroyComponent(bPromoteChildren);
  }
  Super::DestroyComponent(bPromoteChildren);
}

void UIsdkRayVisualComponent::SetCursorMaterial(UMaterialInterface* InMaterial)
{
  static const FName CursorMaterialName(TEXT("Cursor Material"));
  CursorMaterial = InMaterial;
  CursorMaterialInstance = UMaterialInstanceDynamic::Create(InMaterial, this, CursorMaterialName);
  UpdateMaterialInstanceParameters();
  if (Cursor)
  {
    Cursor->SetMaterial(0, CursorMaterialInstance);
  }
}

void UIsdkRayVisualComponent::SetPinchArrowMaterial(UMaterialInterface* InMaterial)
{
  static const FName PinchArrowMaterialName(TEXT("Pinch Arrow Material"));
  PinchArrowMaterial = InMaterial;
  PinchArrowMaterialInstance =
      UMaterialInstanceDynamic::Create(InMaterial, this, PinchArrowMaterialName);
  UpdateMaterialInstanceParameters();
  if (PinchArrow)
  {
    PinchArrow->SetMaterial(0, PinchArrowMaterialInstance);
  }
}

void UIsdkRayVisualComponent::SetTranslucentSortPriority(int32 InTranslucentSortPriority)
{
  TranslucentSortPriority = InTranslucentSortPriority;
  Cursor->SetTranslucentSortPriority(TranslucentSortPriority);
}

void UIsdkRayVisualComponent::UpdateMaterialInstanceParameters()
{
  static const FName AccentColorName(TEXT("AccentColor"));
  static const FName ColorName(TEXT("Color"));
  static const FName ShadowColorName(TEXT("ShadowColor"));

  if (CursorMaterialInstance)
  {
    CursorMaterialInstance->SetVectorParameterValue(AccentColorName, CursorAccentColor);
    CursorMaterialInstance->SetVectorParameterValue(ColorName, CursorColor);
    CursorMaterialInstance->SetVectorParameterValue(ShadowColorName, CursorShadowColor);
  }
  if (PinchArrowMaterialInstance)
  {
    PinchArrowMaterialInstance->SetVectorParameterValue(ColorName, PinchArrowColor);
  }
}

void UIsdkRayVisualComponent::UpdatePinchArrow(
    float PinchStrength,
    const FTransform& PointerPoseTransform)
{
  if (!ShowArrow)
  {
    PinchArrow->SetVisibility(false);
    return;
  }
  PinchArrow->SetVisibility(true);

  for (const FName& MorphName : MorphTargetNames)
  {
    PinchArrow->SetMorphTarget(MorphName, PinchStrength);
  }
  FTransform ModifiedTransform = PointerPoseTransform;
  ModifiedTransform.SetScale3D(PinchArrow->GetComponentScale());
  PinchArrow->SetWorldTransform(ModifiedTransform);

  if (PinchArrowMaterialInstance)
  {
    static const FName SelectStrengthName(TEXT("SelectStrength"));
    PinchArrowMaterialInstance->SetScalarParameterValue(SelectStrengthName, PinchStrength);
  }
}

void UIsdkRayVisualComponent::UpdateCursor(
    float PinchStrength,
    const FTransform& CursorTransform,
    bool CollisionHitValid)
{
  const float CursorSizeByStrength =
      UKismetMathLibrary::Lerp(CursorSizeMinMax.X, CursorSizeMinMax.Y, PinchStrength);
  const float CursorScale = UIsdkRayVisualFunctionLibrary::GetCursorScaleFromDistanceToHmd(
      CursorTransform.GetLocation(), ReferenceDistance, HmdDataSource);
  if (!CollisionHitValid || !ShowCursor)
  {
    Cursor->SetVisibility(false);
    return;
  }
  Cursor->SetVisibility(true);

  Cursor->SetWorldScale3D(FVector(CursorSizeByStrength * CursorScale));
  Cursor->SetWorldLocation(CursorTransform.GetLocation());
  Cursor->SetWorldRotation(CursorTransform.GetRotation());

  if (CursorMaterialInstance)
  {
    static const FName SelectStrengthName(TEXT("SelectStrength"));
    CursorMaterialInstance->SetScalarParameterValue(SelectStrengthName, PinchStrength);
  }
}

void UIsdkRayVisualComponent::UpdateVisual(
    const FTransform& PointerPoseTransform,
    const FTransform& CursorTransform,
    bool CollisionHitValid,
    float RaySelectStrength)
{
  if (!IsVisible())
  {
    return;
  }

  float PinchStrength = RaySelectStrength;
  if (IsValid(PinchArrowStrengthCurve))
  {
    PinchStrength = PinchArrowStrengthCurve->GetFloatValue(PinchStrength);
  }

  UpdatePinchArrow(PinchStrength, PointerPoseTransform);
  UpdateCursor(PinchStrength, CursorTransform, CollisionHitValid);
}

void UIsdkRayVisualComponent::Initialize()
{
  SetCursorMaterial(CursorMaterial);
  SetCursorMesh(CursorMesh);
  SetTranslucentSortPriority(TranslucentSortPriority);
  SetPinchArrowMaterial(PinchArrowMaterial);
  SetPinchArrowMesh(PinchArrowMesh);
}

void UIsdkRayVisualComponent::SetCursorMesh(UStaticMesh* Mesh)
{
  CursorMesh = Mesh;
  Cursor->SetStaticMesh(CursorMesh);
}

void UIsdkRayVisualComponent::SetPinchArrowMesh(USkeletalMesh* Mesh)
{
  PinchArrowMesh = Mesh;
  PinchArrow->SetSkeletalMesh(PinchArrowMesh);
}
