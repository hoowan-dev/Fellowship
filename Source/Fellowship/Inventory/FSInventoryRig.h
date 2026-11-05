// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSInventoryRig.generated.h"

class ACharacter;
class UCameraComponent;
class UFSInventoryComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UPoseableMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FFSInventoryItem;
enum class EFSHand : uint8;

/**
 * What the player sees with the inventory open: a first person camera looking at the held backpack, with the grid floating between them,
 * and the player's forearms raised either side of it with a large cell over each hand.
 * Exists only on the owning client. The grid's cells run from (0, 0) at the top left.
 */
UCLASS()
class FELLOWSHIP_API AFSInventoryRig : public AActor
{
	GENERATED_BODY()

public:
	// The instance custom data each grid cell has, which M_InventoryGridCell reads. Each is 1 or 0.
	static constexpr int32 CellHoveredData = 0;
	static constexpr int32 CellTakenData = 1;
	static constexpr int32 CellCanPlaceData = 2;
	static constexpr int32 CellCannotPlaceData = 3;
	static constexpr int32 CellCanSwapData = 4;
	static constexpr int32 CellDataCount = 5;

	// The instance of the hand slots each hand's cell is
	static constexpr int32 LeftHandSlot = 0;
	static constexpr int32 RightHandSlot = 1;
	static constexpr int32 HandSlotCount = 2;

	// Which hand a hand slot is for
	static EFSHand GetHandOfSlot(int32 HandSlot);

	FORCEINLINE UStaticMesh* GetPackMesh() const { return PackMeshAsset; }
	FORCEINLINE const FRotator& GetPackRotation() const { return PackRotation; }

	AFSInventoryRig();

	virtual void Tick(float DeltaTime) override;

	// Moves the rig to the character's head, facing the way the character faces, and keeps it there. The arms take on the character's mesh.
	void AttachToCharacter(ACharacter* Character);

	void SetPackVisible(bool bVisible);

	// The grid scales in and out rather than appearing instantly
	void SetGridVisible(bool bVisible);

	// Finds the cell a world space ray passes through. Returns false if it misses the grid.
	bool GetCellFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutCell) const;

	// Highlights a cell, or every cell an item covers if there's one in it. While an item is carried, previews the cells it would
	// cover with its top left in this cell instead, showing whether it can go there. Any cell outside the grid clears the highlight.
	void SetHoveredCell(const FIntPoint& Cell);

	// Finds the hand slot a world space ray passes through. Returns false if it misses both.
	bool GetHandSlotFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, int32& OutHandSlot) const;

	// Highlights a hand slot, showing whether the carried item would go in it or swap with what's there. INDEX_NONE clears the highlight.
	void SetHoveredHandSlot(int32 HandSlot);

	// Moves the light and the carried item to where the mouse points, given as a world space ray
	void SetCursorRay(const FVector& RayOrigin, const FVector& RayDirection);

	// Matches the grid, the item meshes, their borders and the carried item to what the inventory holds
	void ShowInventory(const UFSInventoryComponent& Inventory);

	// Where a held item's mesh sits relative to the grid, upright and fitted to the hand's cell
	FTransform GetHandItemTransform(const FFSInventoryItem& Item, int32 HandSlot) const;

	// Where an item's mesh sits relative to the grid, fitted to the cells it covers
	FTransform GetItemTransform(const FFSInventoryItem& Item) const;

	// Where a carried item's mesh sits relative to the grid, with the top left of its cells at a point on the grid. It's closer to the camera
	// than every item in the grid, and shrunk by just enough to look the same size and in the same place as if it were on the grid.
	FTransform GetCarriedItemTransform(const FFSInventoryItem& Item, const FVector2D& GridPoint) const;

	FORCEINLINE const TArray<TObjectPtr<UStaticMeshComponent>>& GetItemMeshes() const { return ItemMeshes; }
	FORCEINLINE UStaticMeshComponent* GetCarriedItemMesh() const { return CarriedItemMesh; }
	FORCEINLINE UStaticMeshComponent* GetHandItemMesh(int32 HandSlot) const { return HandSlot == LeftHandSlot ? LeftHandItemMesh : RightHandItemMesh; }
	FORCEINLINE int32 GetHoveredHandSlot() const { return HoveredHandSlot; }
	FORCEINLINE UPointLightComponent* GetCursorLight() const { return CursorLight; }
	FORCEINLINE const FIntPoint& GetHoveredCell() const { return HoveredCell; }
	FORCEINLINE UInstancedStaticMeshComponent* GetGrid() const { return Grid; }
	FORCEINLINE UInstancedStaticMeshComponent* GetItemBorders() const { return ItemBorders; }
	FORCEINLINE int32 GetCellIndex(const FIntPoint& Cell) const { return Cell.Y * GridWidth + Cell.X; }
	FORCEINLINE UInstancedStaticMeshComponent* GetHandSlots() const { return HandSlots; }
	FORCEINLINE UPoseableMeshComponent* GetArms() const { return Arms; }

	// The centre of a hand's cell on the plane of the grid, in cm from the grid's centre: X to the right and Y up
	FVector2D GetHandSlotCentre(int32 HandSlot) const;

protected:
	virtual void BeginPlay() override;

	void BuildGrid();
	void BuildHandSlots();
	void PoseArms();
	void PoseArm(bool bLeft);
	void FitPackMesh();
	void AddItemBorder(const FFSInventoryItem& Item);
	bool IsCellInGrid(const FIntPoint& Cell) const;

	// Finds where a world space ray meets the plane of the grid, in cm from the grid's centre: X to the right and Y up. Returns false if it never does.
	bool GetGridPointFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FVector2D& OutGridPoint) const;
	FIntPoint GetCellFromGridPoint(const FVector2D& GridPoint) const;
	FTransform GetItemTransformAt(const FFSInventoryItem& Item, const FVector2D& TopLeft) const;
	void HighlightCells(TArray<FIntPoint>&& Cells, int32 CellData);

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UCameraComponent> ViewCamera;

	// Frame the pack and grid are placed in: X runs away from the camera, Y to the right, Z up
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<USceneComponent> Display;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> Pack;

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> Grid;

	// Four bars around each item in the inventory. Attached to the grid so they scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> ItemBorders;

	// Attached to the grid so it scales in and out along with it. Has no mesh while nothing is carried.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> CarriedItemMesh;

	// A large cell over each hand, for what that hand holds. Attached to the grid so they scale in and out along with it.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UInstancedStaticMeshComponent> HandSlots;

	// Attached to the grid so it scales in and out along with it. Has no mesh while the hand is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> LeftHandItemMesh;

	// Attached to the grid so it scales in and out along with it. Has no mesh while the hand is empty.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UStaticMeshComponent> RightHandItemMesh;

	// A copy of the character's mesh with its arms posed in the display's frame and the rest of its body shrunk away, since it's all one mesh
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UPoseableMeshComponent> Arms;

	// Follows the mouse over the grid, shining on the items and the pack. The grid's own cells are unlit, so it doesn't brighten them.
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UPointLightComponent> CursorLight;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.1", Units = cm))
	float CellSize = 3.0f;

	// From the camera to the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "1.0", Units = cm))
	float GridDistance = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid")
	TObjectPtr<UMaterialInterface> GridCellMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid")
	TObjectPtr<UMaterialInterface> ItemBorderMaterial;

	// How wide the border around an item is. It's drawn inside the cells the item covers.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = cm))
	float ItemBorderThickness = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = s))
	float GridScaleTime = 0.08f;

	// How much of the cells it covers an item fills, leaving a gap so it doesn't touch the lines
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ItemFillFraction = 0.9f;

	// The gap between a carried item and the tallest item in the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = cm))
	float CarriedItemClearance = 1.0f;

	// How far in front of the grid the light under the mouse sits
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.0", Units = cm))
	float CursorLightHeight = 8.0f;

	// How far the light reaches, as it's rendered: the meshes it shines on are drawn at FirstPersonScale of their size
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.1", Units = cm))
	float CursorLightRadius = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light", meta = (ClampMin = "0.0"))
	float CursorLightIntensity = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Cursor Light")
	FLinearColor CursorLightColor = FLinearColor(1.0f, 0.85f, 0.65f);

	// Falls back on the grid's cell material if this isn't set
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	TObjectPtr<UMaterialInterface> HandSlotMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "0.1", Units = cm))
	float HandSlotSize = 12.0f;

	// Between the side of the grid and a hand's cell. The hands follow the cells, so they stay clear of the grid whatever its size.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "0.0", Units = cm))
	float HandSlotGap = 3.0f;

	// How far the centre of a hand's cell is above the centre of the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (Units = cm))
	float HandSlotHeight = -2.0f;

	// From the camera to the palms, which sit behind the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "1.0", Units = cm))
	float HandDistance = 65.0f;

	// From the wrist bone to the part of the hand centred in its cell
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands", meta = (ClampMin = "0.0", Units = cm))
	float WristToPalm = 10.0f;

	// The directions below pose the right hand, in the display's frame: X away from the camera, Y to the right, Z up. The left hand mirrors it.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector FingerDirection = FVector(0.15, -0.2, 1.0);

	// The way the palm faces, which only needs to be roughly across the fingers
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector PalmDirection = FVector(-1.0, -0.25, 0.0);

	// From the elbow to the wrist
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector ForearmDirection = FVector(0.4, -0.2, 0.9);

	// From the shoulder to the elbow. The upper arm is kept so that a narrow screen, which shows more below the elbow, doesn't show the arm's cut off end.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Hands")
	FVector UpperArmDirection = FVector(0.55, -0.3, 0.75);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack")
	TObjectPtr<UStaticMesh> PackMeshAsset;

	// From the camera to the centre of the pack
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack", meta = (ClampMin = "1.0", Units = cm))
	float PackDistance = 70.0f;

	// The pack is scaled so its longest side is this long, whatever size the mesh is
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack", meta = (ClampMin = "1.0", Units = cm))
	float PackDisplaySize = 55.0f;

	// Turns the mesh so its front faces the camera, which is towards -X
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Pack")
	FRotator PackRotation = FRotator(0.0f, -90.0f, 0.0f);

	// How far the camera looks down
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|View", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = deg))
	float ViewPitchDown = 20.0f;

	// Added to the head bone's height, which sits low in the head
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|View", meta = (Units = cm))
	float ViewHeightOffset = 10.0f;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ItemMeshes;

	// Replaced by the inventory's size when it's shown
	int32 GridWidth = 8;
	int32 GridHeight = 8;

	// What ShowInventory last showed, used to find the cells a hovered item covers
	TWeakObjectPtr<const UFSInventoryComponent> ShownInventory;

	FIntPoint HoveredCell = FIntPoint(INDEX_NONE, INDEX_NONE);
	int32 HoveredHandSlot = INDEX_NONE;

	// Which of the cell custom data values the hovered hand slot has set
	int32 HoveredHandSlotData = CellHoveredData;

	TArray<FIntPoint> HighlightedCells;

	// Which of the cell custom data values HighlightedCells have set
	int32 HighlightedCellData = CellHoveredData;

	// How far in front of the grid a carried item sits, which ShowInventory finds from the items in the grid
	double CarriedItemLift = 1.0;

	float GridScale = 0.0f;
	float GridScaleTarget = 0.0f;
};
