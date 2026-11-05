// Copyright HoowanDev 2026 all rights reserved.


#include "FSPlayerController.h"
#include "Character/FSCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Inventory/FSInventoryComponent.h"
#include "Inventory/FSInventoryRig.h"
#include "Inventory/FSItemPickup.h"
#include "Player/FSPlayerState.h"
#include "TimerManager.h"

namespace
{
	// How far through the camera blend the worn pack is swapped for the held one. Late on opening and early
	// on closing, so the swap happens while the camera is close to the head and the player can't see it.
	constexpr float OpeningSwapFraction = 0.6f;
	constexpr float ClosingSwapFraction = 0.4f;
}

void AFSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Only the owning client has a local player to receive input
	if (!IsLocalController() || !DefaultMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AFSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InventoryMidpointTimer);
	GetWorldTimerManager().ClearTimer(InventoryFinishedTimer);
	InventoryViewState = EFSInventoryViewState::Closed;

	if (InventoryRig)
	{
		InventoryRig->Destroy();
		InventoryRig = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AFSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput && ToggleInventoryAction)
	{
		EnhancedInput->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AFSPlayerController::ToggleInventory);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory can't be toggled: EnhancedInput=%d ToggleInventoryAction=%s"), EnhancedInput != nullptr, *GetNameSafe(ToggleInventoryAction));
	}

	if (EnhancedInput && InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AFSPlayerController::Interact);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Items can't be picked up: EnhancedInput=%d InteractAction=%s"), EnhancedInput != nullptr, *GetNameSafe(InteractAction));
	}

	if (EnhancedInput && InventorySelectAction)
	{
		EnhancedInput->BindAction(InventorySelectAction, ETriggerEvent::Started, this, &AFSPlayerController::SelectAtInventoryCursor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory items can't be selected: EnhancedInput=%d InventorySelectAction=%s"), EnhancedInput != nullptr, *GetNameSafe(InventorySelectAction));
	}

	if (EnhancedInput && InventoryRotateAction)
	{
		EnhancedInput->BindAction(InventoryRotateAction, ETriggerEvent::Started, this, &AFSPlayerController::RotateCarriedInventoryItem);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Carried items can't be rotated: EnhancedInput=%d InventoryRotateAction=%s"), EnhancedInput != nullptr, *GetNameSafe(InventoryRotateAction));
	}
}

UFSInventoryComponent* AFSPlayerController::GetInventory() const
{
	const AFSPlayerState* FSPlayerState = GetPlayerState<AFSPlayerState>();
	return FSPlayerState ? FSPlayerState->GetInventory() : nullptr;
}

const FFSInventoryItem* AFSPlayerController::GetHoveredInventoryItem() const
{
	const UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !InventoryRig || InventoryViewState != EFSInventoryViewState::Open)
	{
		return nullptr;
	}

	if (InventoryRig->GetHoveredHandSlot() != INDEX_NONE)
	{
		const FFSInventoryItem& HandItem = Inventory->GetHandItem(AFSInventoryRig::GetHandOfSlot(InventoryRig->GetHoveredHandSlot()));
		return HandItem.Definition ? &HandItem : nullptr;
	}

	return Inventory->FindItemAt(InventoryRig->GetHoveredCell());
}

const AFSInventoryRig* AFSPlayerController::GetInventoryRigDefaults() const
{
	return InventoryRigClass ? InventoryRigClass->GetDefaultObject<AFSInventoryRig>() : nullptr;
}

void AFSPlayerController::Interact()
{
	if (InventoryViewState != EFSInventoryViewState::Closed || !FSCharacter)
	{
		return;
	}

	AFSItemPickup* Pickup = FSCharacter->GetNearestPickup();
	UFSInventoryComponent* Inventory = GetInventory();

	if (!Pickup || Pickup->TryPickUp(Inventory))
	{
		return;
	}

	// Checked first so the item stays in the world if the inventory can't open
	if (InventoryRigClass && CanOpenInventory() && Pickup->TryCarry(Inventory))
	{
		OpenInventory();
	}
}

void AFSPlayerController::SelectAtInventoryCursor()
{
	if (InventoryViewState == EFSInventoryViewState::Open)
	{
		UpdateHoveredInventoryCell();

		// Outside the grid means tossing the carried item, so the hands have to be checked first
		if (InventoryRig->GetHoveredHandSlot() != INDEX_NONE)
		{
			SelectInventoryHand(AFSInventoryRig::GetHandOfSlot(InventoryRig->GetHoveredHandSlot()));
		}
		else
		{
			SelectInventoryCell(InventoryRig->GetHoveredCell());
		}
	}
}

void AFSPlayerController::SelectInventoryCell(const FIntPoint& Cell)
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || InventoryViewState != EFSInventoryViewState::Open)
	{
		return;
	}

	if (!Inventory->IsCarryingItem())
	{
		Inventory->PickUpItemAt(Cell);
	}
	else if (Cell == FIntPoint(INDEX_NONE, INDEX_NONE))
	{
		TossCarriedItem();
	}
	else
	{
		Inventory->PlaceCarriedItem(Cell);
	}

	RefreshInventoryView();
}

void AFSPlayerController::SelectInventoryHand(EFSHand Hand)
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (Inventory && InventoryViewState == EFSInventoryViewState::Open && Inventory->SwapCarriedItemWithHand(Hand))
	{
		RefreshInventoryView();
	}
}

void AFSPlayerController::RotateCarriedInventoryItem()
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (Inventory && InventoryViewState == EFSInventoryViewState::Open && Inventory->RotateCarriedItem())
	{
		RefreshInventoryView();
	}
}

void AFSPlayerController::TossCarriedItem()
{
	UFSInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !Inventory->IsCarryingItem() || !FSCharacter)
	{
		return;
	}

	const TSubclassOf<AFSItemPickup> PickupClass = TossedItemClass ? TossedItemClass : TSubclassOf<AFSItemPickup>(AFSItemPickup::StaticClass());

	// Spawning fails while the world is being torn down, and the item is then still on the cursor rather than lost
	if (AFSItemPickup::DropAtFeet(PickupClass, Inventory->GetCarriedItem().Definition, *FSCharacter))
	{
		Inventory->ClearCarriedItem();
	}
}

void AFSPlayerController::RefreshInventoryView()
{
	const UFSInventoryComponent* Inventory = GetInventory();
	if (Inventory && InventoryRig)
	{
		InventoryRig->ShowInventory(*Inventory);
		UpdateHoveredInventoryCell();
	}
}

void AFSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (InventoryViewState != EFSInventoryViewState::Opening && InventoryViewState != EFSInventoryViewState::Open)
	{
		return;
	}

	// The world keeps running, so the character can be knocked off a ledge while looking at the pack
	if (!CanOpenInventory())
	{
		CloseInventory();
		return;
	}

	if (InventoryViewState == EFSInventoryViewState::Open)
	{
		UpdateHoveredInventoryCell();
	}
}

void AFSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	FSCharacter = Cast<AFSCharacter>(InPawn);
}

void AFSPlayerController::OnUnPossess()
{
	CloseInventoryImmediately();

	Super::OnUnPossess();

	FSCharacter = nullptr;
}

void AFSPlayerController::ToggleInventory()
{
	// Toggling during a blend is ignored, because reversing a camera blend part way through snaps it to the end
	if (InventoryViewState == EFSInventoryViewState::Closed)
	{
		OpenInventory();
	}
	else if (InventoryViewState == EFSInventoryViewState::Open)
	{
		CloseInventory();
	}
}

bool AFSPlayerController::CanOpenInventory() const
{
	const UCharacterMovementComponent* Movement = FSCharacter ? FSCharacter->GetCharacterMovement() : nullptr;
	return Movement && Movement->IsMovingOnGround();
}

void AFSPlayerController::OpenInventory()
{
	if (InventoryViewState != EFSInventoryViewState::Closed || !CanOpenInventory())
	{
		return;
	}

	if (!InventoryRigClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Inventory can't open: InventoryRigClass isn't set"));
		return;
	}

	if (!InventoryRig)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		InventoryRig = GetWorld()->SpawnActor<AFSInventoryRig>(InventoryRigClass, SpawnParameters);
	}

	InventoryRig->AttachToCharacter(FSCharacter);

	if (const UFSInventoryComponent* Inventory = GetInventory())
	{
		InventoryRig->ShowInventory(*Inventory);
	}

	InventoryViewState = EFSInventoryViewState::Opening;
	SetInventoryInputActive(true);
	StartInventoryBlend(InventoryRig, OpeningSwapFraction);
}

void AFSPlayerController::CloseInventory()
{
	if (InventoryViewState != EFSInventoryViewState::Opening && InventoryViewState != EFSInventoryViewState::Open)
	{
		return;
	}

	TossCarriedItem();
	RefreshInventoryView();

	InventoryViewState = EFSInventoryViewState::Closing;
	InventoryRig->SetGridVisible(false);
	SetInventoryInputActive(false);
	StartInventoryBlend(FSCharacter, ClosingSwapFraction);
}

void AFSPlayerController::CloseInventoryImmediately()
{
	if (InventoryViewState == EFSInventoryViewState::Closed)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(InventoryMidpointTimer);
	GetWorldTimerManager().ClearTimer(InventoryFinishedTimer);

	TossCarriedItem();
	RefreshInventoryView();

	InventoryRig->SetGridVisible(false);
	SetInventoryInputActive(false);
	SwapWornPackForHeldPack(false);
	InventoryRig->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetViewTarget(FSCharacter);

	InventoryViewState = EFSInventoryViewState::Closed;
}

void AFSPlayerController::SetInventoryInputActive(bool bActive)
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		UInputMappingContext* ContextToAdd = bActive ? InventoryMappingContext : DefaultMappingContext;
		UInputMappingContext* ContextToRemove = bActive ? DefaultMappingContext : InventoryMappingContext;

		if (ContextToRemove)
		{
			Subsystem->RemoveMappingContext(ContextToRemove);
		}

		if (ContextToAdd)
		{
			Subsystem->AddMappingContext(ContextToAdd, 0);
		}
	}

	bShowMouseCursor = bActive;

	if (bActive)
	{
		// Without this the cursor disappears whenever a mouse button is held
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);

		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		GetViewportSize(ViewportWidth, ViewportHeight);
		SetMouseLocation(ViewportWidth / 2, ViewportHeight / 2);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void AFSPlayerController::StartInventoryBlend(AActor* NewViewTarget, float MidpointFraction)
{
	SetViewTargetWithBlend(NewViewTarget, InventoryBlendTime, VTBlend_EaseInOut, 2.0f);

	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.SetTimer(InventoryMidpointTimer, this, &AFSPlayerController::HandleInventoryBlendMidpoint, InventoryBlendTime * MidpointFraction, false);
	TimerManager.SetTimer(InventoryFinishedTimer, this, &AFSPlayerController::HandleInventoryBlendFinished, InventoryBlendTime, false);
}

void AFSPlayerController::SwapWornPackForHeldPack(bool bHeld)
{
	if (FSCharacter)
	{
		FSCharacter->SetInventoryViewBonesHidden(bHeld);
	}

	InventoryRig->SetPackVisible(bHeld);
}

void AFSPlayerController::HandleInventoryBlendMidpoint()
{
	SwapWornPackForHeldPack(InventoryViewState == EFSInventoryViewState::Opening);
}

void AFSPlayerController::HandleInventoryBlendFinished()
{
	if (InventoryViewState == EFSInventoryViewState::Opening)
	{
		InventoryViewState = EFSInventoryViewState::Open;
		InventoryRig->SetGridVisible(true);
	}
	else if (InventoryViewState == EFSInventoryViewState::Closing)
	{
		InventoryViewState = EFSInventoryViewState::Closed;
		InventoryRig->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

void AFSPlayerController::UpdateHoveredInventoryCell()
{
	FVector RayOrigin;
	FVector RayDirection;
	FIntPoint Cell(INDEX_NONE, INDEX_NONE);
	int32 HandSlot = INDEX_NONE;

	if (DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		InventoryRig->GetCellFromWorldRay(RayOrigin, RayDirection, Cell);
		InventoryRig->GetHandSlotFromWorldRay(RayOrigin, RayDirection, HandSlot);
		InventoryRig->SetCursorRay(RayOrigin, RayDirection);
	}

	InventoryRig->SetHoveredCell(Cell);
	InventoryRig->SetHoveredHandSlot(HandSlot);
}
