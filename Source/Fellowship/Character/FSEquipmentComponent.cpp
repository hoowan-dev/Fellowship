// Copyright HoowanDev 2026 all rights reserved.

#include "Character/FSEquipmentComponent.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Inventory/FSItemDefinition.h"
#include "ReferenceSkeleton.h"

namespace
{
	// A long frame is simulated as if it were this long, because a spring given too long a step flies apart
	constexpr double MaxSwingStep = 1.0 / 30.0;

	// A swing past this far would flip what's hanging over, however hard a landing or teleport was
	constexpr double SwingAngleLimit = 60.0;

	// A little different for each mesh, so that they don't all swing as one
	constexpr double HandSwingStiffnessScales[] = { 1.0, 0.88 };
	constexpr double HungItemSwingStiffnessScales[] = { 1.06, 0.94 };

	// Refreshes right away, because a poseable mesh's own tick comes before the character has been animated, which would leave it a frame behind
	void CopyCharacterPose(UPoseableMeshComponent& Mesh, USkeletalMeshComponent& CharacterMesh)
	{
		Mesh.CopyPoseFromSkeletalComponent(&CharacterMesh);
		Mesh.RefreshBoneTransforms();
	}

	// A bone's rotation relative to the mesh, in the pose the mesh was modeled in
	FQuat GetBoneRefPoseRotation(const USkeletalMeshComponent& Mesh, FName Bone)
	{
		const USkeletalMesh* Asset = Mesh.GetSkeletalMeshAsset();
		if (!Asset)
		{
			return FQuat::Identity;
		}

		const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
		FQuat Rotation = FQuat::Identity;

		for (int32 BoneIndex = Skeleton.FindBoneIndex(Bone); BoneIndex != INDEX_NONE; BoneIndex = Skeleton.GetParentIndex(BoneIndex))
		{
			Rotation = Skeleton.GetRefBonePose()[BoneIndex].GetRotation() * Rotation;
		}

		return Rotation;
	}
}

UFSEquipmentComponent::UFSEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// After the character has moved and been animated, so that the swing starts from where it's being drawn this frame
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UFSEquipmentComponent::FSwing::Settle()
{
	Angle = FVector2D::ZeroVector;
	AngleSpeed = FVector2D::ZeroVector;
	bHasLastAttachLocation = false;
}

void UFSEquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	for (const EFSHand Hand : { EFSHand::Left, EFSHand::Right })
	{
		const int32 Index = static_cast<int32>(Hand);

		// Weapons only swing on the back. In the hands, they go where the animation takes them.
		if (bDrawn)
		{
			HandSwings[Index].Settle();
		}
		else
		{
			UpdateSwing(HandSwings[Index], HandMeshes[Index], DeltaTime, HandSwingStiffnessScales[Index]);
		}
	}

	for (const EFSQuickSlot QuickSlot : { EFSQuickSlot::Left, EFSQuickSlot::Right })
	{
		const int32 Index = static_cast<int32>(QuickSlot);
		UpdateSwing(HungItemSwings[Index], HungItemMeshes.Get(QuickSlot), DeltaTime, HungItemSwingStiffnessScales[Index]);
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;

	for (UPoseableMeshComponent* ArmorMesh : SkeletalArmorMeshes)
	{
		if (CharacterMesh && ArmorMesh && ArmorMesh->GetSkinnedAsset() && ArmorMesh->IsVisible())
		{
			CopyCharacterPose(*ArmorMesh, *CharacterMesh);
		}
	}
}

void UFSEquipmentComponent::UpdateSwing(FSwing& Swing, UStaticMeshComponent* Mesh, float DeltaTime, double StiffnessScale) const
{
	const USceneComponent* AttachParent = Mesh ? Mesh->GetAttachParent() : nullptr;

	if (!AttachParent || !Mesh->GetStaticMesh() || bMeshesHidden)
	{
		Swing.Settle();
		return;
	}

	const FTransform Attach = AttachParent->GetSocketTransform(Mesh->GetAttachSocketName());
	const FVector AttachLocation = Attach.GetLocation();

	// Measured at the attachment, so it takes in the character's walk and its turning as well as where it's going
	const FVector Move = Swing.bHasLastAttachLocation ? Attach.InverseTransformVectorNoScale(AttachLocation - Swing.LastAttachLocation) : FVector::ZeroVector;
	Swing.LastAttachLocation = AttachLocation;
	Swing.bHasLastAttachLocation = true;

	// The hanging end trails behind the way it's being carried
	const FVector2D TargetAngle = (FVector2D(-Move.X, -Move.Y) / DeltaTime * SwingPerSpeed).ClampAxes(-MaxSwing, MaxSwing);
	const double Step = FMath::Min<double>(DeltaTime, MaxSwingStep);

	// A damped spring, so that it overshoots a little and swings back when the character stops. Speed comes first, which keeps it stable.
	Swing.AngleSpeed += ((TargetAngle - Swing.Angle) * SwingStiffness * StiffnessScale - Swing.AngleSpeed * SwingDamping) * Step;
	Swing.Angle = (Swing.Angle + Swing.AngleSpeed * Step).ClampAxes(-SwingAngleLimit, SwingAngleLimit);

	const FVector HangingEnd(FMath::Tan(FMath::DegreesToRadians(Swing.Angle.X)), FMath::Tan(FMath::DegreesToRadians(Swing.Angle.Y)), -1.0);
	const FQuat Turn = FQuat::FindBetweenNormals(FVector::DownVector, HangingEnd.GetSafeNormal());

	Mesh->SetRelativeTransform(FTransform(
		Turn * Swing.Rest.GetRotation(),
		Swing.Pivot + Turn.RotateVector(Swing.Rest.GetLocation() - Swing.Pivot),
		Swing.Rest.GetScale3D()));
}

void UFSEquipmentComponent::SetInventory(UFSInventoryComponent* NewInventory)
{
	if (Inventory == NewInventory)
	{
		return;
	}

	if (Inventory)
	{
		Inventory->OnHandItemsChanged.RemoveAll(this);
		Inventory->OnBackpackChanged.RemoveAll(this);
		Inventory->OnQuickSlotItemsChanged.RemoveAll(this);
		Inventory->OnArmorChanged.RemoveAll(this);
	}

	Inventory = NewInventory;

	if (Inventory)
	{
		Inventory->OnHandItemsChanged.AddUObject(this, &UFSEquipmentComponent::HandleHandItemsChanged);
		Inventory->OnBackpackChanged.AddUObject(this, &UFSEquipmentComponent::RefreshBackpackMesh);
		Inventory->OnQuickSlotItemsChanged.AddUObject(this, &UFSEquipmentComponent::RefreshBackpackMesh);
		Inventory->OnArmorChanged.AddUObject(this, &UFSEquipmentComponent::RefreshArmorMeshes);
	}

	HandleHandItemsChanged();
}

void UFSEquipmentComponent::ToggleDrawn()
{
	if (bDrawn)
	{
		Sheathe();
	}
	else
	{
		Draw();
	}
}

bool UFSEquipmentComponent::Draw()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UFSCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement<UFSCharacterMovementComponent>() : nullptr;

	if (!IsHoldingAnything() || (Movement && Movement->IsClimbing()))
	{
		return false;
	}

	bDrawn = true;
	RefreshMeshes();
	return true;
}

void UFSEquipmentComponent::Sheathe()
{
	bDrawn = false;
	RefreshMeshes();
}

const UFSWeaponStance* UFSEquipmentComponent::GetCurrentStance() const
{
	if (bDrawn)
	{
		const EFSWeaponType RightHand = GetHeldWeaponType(EFSHand::Right);
		const EFSWeaponType LeftHand = GetHeldWeaponType(EFSHand::Left);

		for (const FFSWeaponStanceRule& Rule : StanceRules)
		{
			if (Rule.Stance && Rule.RightHand == RightHand && Rule.LeftHand == LeftHand)
			{
				return Rule.Stance;
			}
		}
	}

	return UnarmedStance;
}

void UFSEquipmentComponent::SetMeshesHidden(bool bHidden)
{
	bMeshesHidden = bHidden;
	RefreshMeshes();
}

void UFSEquipmentComponent::HandleHandItemsChanged()
{
	// Otherwise the next weapon put in a hand would be drawn without the player asking
	if (!IsHoldingAnything())
	{
		bDrawn = false;
	}

	RefreshMeshes();
}

void UFSEquipmentComponent::RefreshMeshes()
{
	RefreshMesh(EFSHand::Right);
	RefreshMesh(EFSHand::Left);
	RefreshBackpackMesh();
	RefreshArmorMeshes();
}

void UFSEquipmentComponent::RefreshArmorMeshes()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return;
	}

	for (const EFSArmorType ArmorType : { EFSArmorType::Head, EFSArmorType::Face, EFSArmorType::Body })
	{
		RefreshArmorMesh(ArmorType, *CharacterMesh);
	}

	RefreshHiddenBones(*CharacterMesh);
}

void UFSEquipmentComponent::RefreshArmorMesh(EFSArmorType ArmorType, USkeletalMeshComponent& CharacterMesh)
{
	const int32 Index = static_cast<int32>(ArmorType);
	const UFSItemDefinition* Item = Inventory ? Inventory->GetArmorItem(ArmorType).Definition.Get() : nullptr;

	// What replaces the head leaves no face to wear anything on
	const bool bShown = Item && !(ArmorType == EFSArmorType::Face && IsArmorReplacingPart(EFSArmorType::Head));
	const bool bVisible = !bMeshesHidden || ArmorType == EFSArmorType::Body;

	UStaticMesh* StaticMesh = bShown && !Item->bUsesSkeletalMesh ? Item->Mesh.Get() : nullptr;
	USkeletalMesh* SkeletalMesh = bShown && Item->bUsesSkeletalMesh ? Item->SkeletalMesh.Get() : nullptr;

	TObjectPtr<UStaticMeshComponent>& ArmorMesh = ArmorMeshes[Index];

	if (StaticMesh)
	{
		if (!ArmorMesh)
		{
			ArmorMesh = CreateAttachedMesh(&CharacterMesh);
		}

		const FName Bone = ArmorType == EFSArmorType::Body ? BodyArmorBone : HeadBone;

		// A missing bone attaches to the mesh's root without complaint, leaving the armor at the character's feet
		if (CharacterMesh.GetSkeletalMeshAsset() && !CharacterMesh.DoesSocketExist(Bone))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s has no socket or bone called %s for armor to attach to"), *GetNameSafe(CharacterMesh.GetSkeletalMeshAsset()), *Bone.ToString());
		}

		ArmorMesh->SetStaticMesh(StaticMesh);
		ArmorMesh->AttachToComponent(&CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bone);

		// Bones have axes of their own, whereas armor is modeled in the axes of the character's mesh
		ArmorMesh->SetRelativeTransform(Item->WornTransform * FTransform(GetBoneRefPoseRotation(CharacterMesh, Bone).Inverse()));
		ArmorMesh->SetVisibility(bVisible);
	}
	else if (ArmorMesh)
	{
		ArmorMesh->SetStaticMesh(nullptr);
		ArmorMesh->SetVisibility(false);
	}

	TObjectPtr<UPoseableMeshComponent>& SkeletalArmorMesh = SkeletalArmorMeshes[Index];

	if (SkeletalMesh)
	{
		if (!SkeletalArmorMesh)
		{
			SkeletalArmorMesh = NewObject<UPoseableMeshComponent>(GetOwner());
			SkeletalArmorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SkeletalArmorMesh->SetCanEverAffectNavigation(false);
			SkeletalArmorMesh->SetupAttachment(&CharacterMesh);
			SkeletalArmorMesh->RegisterComponent();
		}

		if (SkeletalArmorMesh->GetSkinnedAsset() != SkeletalMesh)
		{
			// Through nothing, because a poseable mesh only sets up for a new mesh's bones if there's a different number of them
			SkeletalArmorMesh->SetSkinnedAssetAndUpdate(nullptr);
			SkeletalArmorMesh->SetSkinnedAssetAndUpdate(SkeletalMesh);
		}

		// Now, because it would otherwise spend a frame in its reference pose
		CopyCharacterPose(*SkeletalArmorMesh, CharacterMesh);
		SkeletalArmorMesh->SetVisibility(bVisible);
	}
	else if (SkeletalArmorMesh)
	{
		SkeletalArmorMesh->SetSkinnedAssetAndUpdate(nullptr);
		SkeletalArmorMesh->SetVisibility(false);
	}
}

void UFSEquipmentComponent::RefreshHiddenBones(USkeletalMeshComponent& CharacterMesh)
{
	// Done every time because the inventory view shows this bone again as it closes, if it's set up to hide it. Bones keep their
	// transforms while hidden, so the worn mesh still follows it.
	CharacterMesh.HideBoneByName(BackpackBone, PBO_None);

	const bool bReplaceHead = IsArmorReplacingPart(EFSArmorType::Head);
	const bool bReplaceBody = IsArmorReplacingPart(EFSArmorType::Body);

	if (bReplaceHead)
	{
		CharacterMesh.HideBoneByName(HeadBone, PBO_None);
	}
	else if (bHeadReplaced && !bMeshesHidden)
	{
		// While the meshes are hidden, the head is too until the inventory view shows it again
		CharacterMesh.UnHideBoneByName(HeadBone);
	}

	if (bReplaceBody)
	{
		CharacterMesh.HideBoneByName(BodyBone, PBO_None);
	}
	else if (bBodyReplaced)
	{
		CharacterMesh.UnHideBoneByName(BodyBone);
	}

	bHeadReplaced = bReplaceHead;
	bBodyReplaced = bReplaceBody;

	const USkeletalMesh* Asset = CharacterMesh.GetSkeletalMeshAsset();
	const int32 HeadBoneIndex = Asset ? Asset->GetRefSkeleton().FindBoneIndex(HeadBone) : INDEX_NONE;

	if (!bReplaceBody || HeadBoneIndex == INDEX_NONE)
	{
		return;
	}

	// Hiding a bone hides everything below it, which would take the head with the body. Nothing rebuilds these states until the next time a bone is hidden or shown, which sets them again
	// by coming through here, and the bones above the head stay hidden because some of the body is skinned to them. The head's vertices are only skinned to the head, so it isn't affected.
	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	TArray<uint8>& States = CharacterMesh.GetEditableBoneVisibilityStates();

	for (int32 Bone = HeadBoneIndex; Bone < States.Num(); ++Bone)
	{
		const bool bBelowShownBone = Bone != HeadBoneIndex && Skeleton.BoneIsChildOf(Bone, HeadBoneIndex) && States[Skeleton.GetParentIndex(Bone)] == BVS_Visible;

		if (States[Bone] == BVS_HiddenByParent && (Bone == HeadBoneIndex || bBelowShownBone))
		{
			States[Bone] = BVS_Visible;
		}
	}
}

bool UFSEquipmentComponent::IsArmorReplacingPart(EFSArmorType ArmorType) const
{
	const UFSItemDefinition* Item = Inventory ? Inventory->GetArmorItem(ArmorType).Definition.Get() : nullptr;
	return Item && Item->ReplacesCharacterPart();
}

void UFSEquipmentComponent::RefreshBackpackMesh()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return;
	}

	RefreshHiddenBones(*CharacterMesh);

	const UFSItemDefinition* Backpack = Inventory ? Inventory->GetBackpack() : nullptr;
	UStaticMesh* Mesh = Backpack ? Backpack->Mesh.Get() : nullptr;

	if (!Mesh)
	{
		if (BackpackMesh)
		{
			BackpackMesh->SetStaticMesh(nullptr);
			BackpackMesh->SetVisibility(false);
			HungItemMeshes.Show(*BackpackMesh, nullptr, nullptr, nullptr);
		}
		return;
	}

	if (!BackpackMesh)
	{
		BackpackMesh = CreateAttachedMesh(CharacterMesh);
	}

	// A missing bone attaches to the mesh's root without complaint, leaving the backpack at the character's feet
	if (CharacterMesh->GetSkeletalMeshAsset() && !CharacterMesh->DoesSocketExist(BackpackBone))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no socket or bone called %s for a backpack to attach to"), *GetNameSafe(CharacterMesh->GetSkeletalMeshAsset()), *BackpackBone.ToString());
	}

	BackpackMesh->SetStaticMesh(Mesh);
	BackpackMesh->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, BackpackBone);
	BackpackMesh->SetRelativeTransform(FTransform::Identity);
	BackpackMesh->SetVisibility(!bMeshesHidden);

	HungItemMeshes.Show(*BackpackMesh, Backpack, Inventory->GetQuickSlotItem(EFSQuickSlot::Left).Definition, Inventory->GetQuickSlotItem(EFSQuickSlot::Right).Definition, HungItemMaxDrop);

	for (const EFSQuickSlot QuickSlot : { EFSQuickSlot::Left, EFSQuickSlot::Right })
	{
		FSwing& Swing = HungItemSwings[static_cast<int32>(QuickSlot)];
		Swing.Rest = HungItemMeshes.GetRest(QuickSlot);
		Swing.Pivot = HungItemMeshes.GetPivot(QuickSlot);
	}
}

UStaticMeshComponent* UFSEquipmentComponent::CreateAttachedMesh(USkeletalMeshComponent* CharacterMesh) const
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetupAttachment(CharacterMesh);
	Mesh->RegisterComponent();
	return Mesh;
}

void UFSEquipmentComponent::RefreshMesh(EFSHand Hand)
{
	const UFSItemDefinition* Item = Inventory ? Inventory->GetHandItem(Hand).Definition.Get() : nullptr;
	const bool bShown = Item && (bDrawn ? Item->bShowHandEquipped : Item->bShowHandStored);
	UStaticMesh* Mesh = bShown ? Item->Mesh.Get() : nullptr;
	TObjectPtr<UStaticMeshComponent>& HandMesh = HandMeshes[static_cast<int32>(Hand)];

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;

	if (!Mesh || !CharacterMesh)
	{
		if (HandMesh)
		{
			HandMesh->SetStaticMesh(nullptr);
			HandMesh->SetVisibility(false);
		}
		return;
	}

	if (!HandMesh)
	{
		HandMesh = CreateAttachedMesh(CharacterMesh);
	}

	const bool bRight = Hand == EFSHand::Right;
	const FName Socket = bDrawn ? (bRight ? RightHandSocket : LeftHandSocket) : (bRight ? RightSheathSocket : LeftSheathSocket);

	// A missing socket attaches to the mesh's root without complaint, leaving the weapon at the character's feet
	if (CharacterMesh->GetSkeletalMeshAsset() && !CharacterMesh->DoesSocketExist(Socket))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no socket or bone called %s for a weapon to attach to"), *GetNameSafe(CharacterMesh->GetSkeletalMeshAsset()), *Socket.ToString());
	}

	HandMesh->SetStaticMesh(Mesh);
	HandMesh->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	const FTransform& ItemTransform = bDrawn ? Item->HandEquippedTransform : Item->HandStoredTransform;
	const FTransform& HandTransform = bDrawn ? (bRight ? RightHeldOffset : LeftHeldOffset) : (bRight ? RightSheathOffset : LeftSheathOffset);
	HandMesh->SetRelativeTransform(ItemTransform * HandTransform);
	HandMesh->SetVisibility(!bMeshesHidden);

	// It swings about its grip, which is the mesh's pivot. A different socket is a different place to measure from.
	FSwing& Swing = HandSwings[static_cast<int32>(Hand)];
	Swing.Rest = HandMesh->GetRelativeTransform();
	Swing.Pivot = Swing.Rest.GetLocation();
	Swing.bHasLastAttachLocation = false;
}

bool UFSEquipmentComponent::IsHoldingAnything() const
{
	return Inventory && (Inventory->GetHandItem(EFSHand::Right).Definition || Inventory->GetHandItem(EFSHand::Left).Definition);
}

EFSWeaponType UFSEquipmentComponent::GetHeldWeaponType(EFSHand Hand) const
{
	const UFSItemDefinition* Item = Inventory ? Inventory->GetHandItem(Hand).Definition.Get() : nullptr;
	if (!Item)
	{
		return EFSWeaponType::None;
	}

	// Anything that isn't a weapon is held the way a one-handed weapon is, so it shares those stance rules
	return Item->ItemType == EFSItemType::Weapon ? Item->WeaponType : EFSWeaponType::Sword;
}
