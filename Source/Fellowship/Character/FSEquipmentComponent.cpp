// Copyright HoowanDev 2026 all rights reserved.

#include "Character/FSEquipmentComponent.h"
#include "Character/FSCharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"

UFSEquipmentComponent::UFSEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	}

	Inventory = NewInventory;

	if (Inventory)
	{
		Inventory->OnHandItemsChanged.AddUObject(this, &UFSEquipmentComponent::HandleHandItemsChanged);
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
}

void UFSEquipmentComponent::RefreshMesh(EFSHand Hand)
{
	const UFSItemDefinition* Item = Inventory ? Inventory->GetHandItem(Hand).Definition.Get() : nullptr;
	UStaticMesh* Mesh = Item ? Item->Mesh.Get() : nullptr;
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
		HandMesh = NewObject<UStaticMeshComponent>(GetOwner());
		HandMesh->SetMobility(EComponentMobility::Movable);
		HandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HandMesh->SetCanEverAffectNavigation(false);
		HandMesh->SetupAttachment(CharacterMesh);
		HandMesh->RegisterComponent();
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
	const FTransform& ItemTransform = bDrawn ? Item->HeldTransform : Item->SheathedTransform;
	const FTransform& HandTransform = bDrawn ? (bRight ? RightHeldOffset : LeftHeldOffset) : (bRight ? RightSheathOffset : LeftSheathOffset);
	HandMesh->SetRelativeTransform(ItemTransform * HandTransform);
	HandMesh->SetVisibility(!bMeshesHidden);
}

bool UFSEquipmentComponent::IsHoldingAnything() const
{
	return Inventory && (Inventory->GetHandItem(EFSHand::Right).Definition || Inventory->GetHandItem(EFSHand::Left).Definition);
}

EFSWeaponType UFSEquipmentComponent::GetHeldWeaponType(EFSHand Hand) const
{
	const UFSItemDefinition* Item = Inventory ? Inventory->GetHandItem(Hand).Definition.Get() : nullptr;
	return Item ? Item->WeaponType : EFSWeaponType::None;
}
