// Copyright HoowanDev 2026 all rights reserved.


#include "Inventory/FSLootContainer.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/FSEventLog.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSPlayerController.h"
#include "TimerManager.h"

AFSLootContainer::AFSLootContainer()
{
	InteractVerb = NSLOCTEXT("FSLootContainer", "DefaultInteractVerb", "Open");

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(GetArea());
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(true);

	ContentsArea = CreateDefaultSubobject<UBoxComponent>(TEXT("ContentsArea"));
	ContentsArea->SetupAttachment(GetArea());
	ContentsArea->InitBoxExtent(FVector(30.0f, 30.0f, 5.0f));
	ContentsArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContentsArea->SetGenerateOverlapEvents(false);
	ContentsArea->SetCanEverAffectNavigation(false);
	ContentsArea->SetHiddenInGame(true);
	ContentsArea->ShapeColor = FColor(255, 200, 80);
}

void AFSLootContainer::Interact_Implementation(AFSPlayerController* PlayerController)
{
	if (bOpen)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Opening loot container %s with %d items"), *GetActorNameOrLabel(), Items.Num());
	UFSEventLog::Add(this, FText::Format(NSLOCTEXT("FSLootContainer", "OpenedEvent", "Opened {0}"), DisplayName));

	bOpen = true;
	if (OpenAnimation)
	{
		Mesh->PlayAnimation(OpenAnimation, false);
	}

	SpawnItemPickups();

	Looter = PlayerController;
	if (!PlayerController)
	{
		return;
	}

	// The inventory opens once the lid is up
	const float LidTime = OpenAnimation ? OpenAnimation->GetPlayLength() : 0.0f;
	if (LidTime > 0.0f)
	{
		GetWorldTimerManager().SetTimer(OpenInventoryTimer, this, &AFSLootContainer::OpenInventory, LidTime, false);
	}
	else
	{
		OpenInventory();
	}
}

void AFSLootContainer::OpenInventory()
{
	AFSPlayerController* PlayerController = Looter.Get();
	if (!bOpen || !PlayerController)
	{
		Close();
		return;
	}

	PlayerController->OpenInventory();

	// It can't open in the air, such as after being knocked off a ledge while the lid was going up
	if (PlayerController->GetInventoryViewState() == EFSInventoryViewState::Closed)
	{
		Close();
		return;
	}

	InventoryClosedHandle = PlayerController->OnInventoryClosed.AddUObject(this, &AFSLootContainer::Close);
}

void AFSLootContainer::Close()
{
	if (!bOpen)
	{
		return;
	}

	StopWatchingInventory();
	bOpen = false;

	// What wasn't taken goes back inside. That includes one the player tried to take without room for it, which was put back as a new pickup this owns.
	for (AFSItemPickup* Pickup : GetItemPickups())
	{
		if (Pickup->Item)
		{
			Items.Add(Pickup->Item);
		}

		Pickup->Destroy();
	}

	if (CloseAnimation)
	{
		Mesh->PlayAnimation(CloseAnimation, false);
	}

	UE_LOG(LogTemp, Log, TEXT("Closed loot container %s with %d items left"), *GetActorNameOrLabel(), Items.Num());
}

FFSItemText AFSLootContainer::GetInteractionLabel() const
{
	// Open, it has nothing to say: what's inside is in the inventory's nearby list
	return bOpen ? FFSItemText() : Super::GetInteractionLabel();
}

FText AFSLootContainer::GetInteractionVerb(const UFSInventoryComponent* Inventory) const
{
	return bOpen ? FText::GetEmpty() : Super::GetInteractionVerb(Inventory);
}

void AFSLootContainer::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	// Walking out of its area, inventory open or not, closes it, taking back what's inside
	const AFSPlayerController* PlayerController = Looter.Get();
	if (bOpen && PlayerController && OtherActor && OtherActor == PlayerController->GetPawn())
	{
		Close();
	}
}

TArray<AFSItemPickup*> AFSLootContainer::GetItemPickups() const
{
	TArray<AFSItemPickup*> Pickups;
	for (TActorIterator<AFSItemPickup> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == this && !It->IsActorBeingDestroyed())
		{
			Pickups.Add(*It);
		}
	}

	return Pickups;
}

void AFSLootContainer::SpawnItemPickups()
{
	TArray<UFSItemDefinition*> ToSpawn;
	for (UFSItemDefinition* Item : Items)
	{
		if (Item)
		{
			ToSpawn.Add(Item);
		}
	}

	Items.Reset();
	if (ToSpawn.IsEmpty())
	{
		return;
	}

	// Rows across the bottom of the area, as near square as the count allows
	const int32 Columns = FMath::CeilToInt32(FMath::Sqrt(static_cast<float>(ToSpawn.Num())));
	const int32 Rows = FMath::DivideAndRoundUp(ToSpawn.Num(), Columns);
	const FVector Extent = ContentsArea->GetUnscaledBoxExtent();
	const FTransform AreaTransform = ContentsArea->GetComponentTransform();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TSubclassOf<AFSItemPickup> PickupClass = ItemPickupClass ? ItemPickupClass : TSubclassOf<AFSItemPickup>(AFSItemPickup::StaticClass());

	for (int32 Index = 0; Index < ToSpawn.Num(); ++Index)
	{
		UFSItemDefinition* Item = ToSpawn[Index];
		const int32 Column = Index % Columns;
		const int32 Row = Index / Columns;
		const FVector OnBottom(
			-Extent.X + (Row + 0.5) * 2.0 * Extent.X / Rows,
			-Extent.Y + (Column + 0.5) * 2.0 * Extent.Y / Columns,
			-Extent.Z);

		// Resting on the bottom, as a dropped item rests on the floor
		const FVector Location = AreaTransform.TransformPosition(OnBottom) + FVector(0.0, 0.0, AFSItemPickup::GetFloorClearance(*Item));
		const FTransform SpawnTransform(FRotator(0.0, AreaTransform.Rotator().Yaw, 0.0), Location);

		if (AFSItemPickup* Pickup = GetWorld()->SpawnActor<AFSItemPickup>(PickupClass, SpawnTransform, SpawnParameters))
		{
			// Only in the inventory's nearby list, not in the world
			Pickup->SetItem(Item);
			Pickup->SetShownInWorld(false);
		}
		else
		{
			// Kept rather than lost if it can't be spawned
			Items.Add(Item);
		}
	}
}

void AFSLootContainer::StopWatchingInventory()
{
	GetWorldTimerManager().ClearTimer(OpenInventoryTimer);

	if (AFSPlayerController* PlayerController = Looter.Get())
	{
		PlayerController->OnInventoryClosed.Remove(InventoryClosedHandle);
	}

	InventoryClosedHandle.Reset();
	Looter.Reset();
}

void AFSLootContainer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopWatchingInventory();

	Super::EndPlay(EndPlayReason);
}
