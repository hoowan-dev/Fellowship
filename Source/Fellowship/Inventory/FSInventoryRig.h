// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSInventoryRig.generated.h"

class ACharacter;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * What the player sees with the inventory open: a first person camera looking at the held backpack, with the grid floating between them.
 * Exists only on the owning client. The grid's cells run from (0, 0) at the top left.
 */
UCLASS()
class FELLOWSHIP_API AFSInventoryRig : public AActor
{
	GENERATED_BODY()

public:
	AFSInventoryRig();

	virtual void Tick(float DeltaTime) override;

	// Moves the rig to the character's head, facing the way the character faces, and keeps it there
	void AttachToCharacter(ACharacter* Character);

	void SetPackVisible(bool bVisible);

	// The grid scales in and out rather than appearing instantly
	void SetGridVisible(bool bVisible);

	// Finds the cell a world space ray passes through. Returns false if it misses the grid.
	bool GetCellFromWorldRay(const FVector& RayOrigin, const FVector& RayDirection, FIntPoint& OutCell) const;

	// Highlights one cell. Any cell outside the grid clears the highlight.
	void SetHoveredCell(const FIntPoint& Cell);

	FORCEINLINE const FIntPoint& GetHoveredCell() const { return HoveredCell; }
	FORCEINLINE UInstancedStaticMeshComponent* GetGrid() const { return Grid; }
	FORCEINLINE int32 GetCellIndex(const FIntPoint& Cell) const { return Cell.Y * GridWidth + Cell.X; }

protected:
	virtual void BeginPlay() override;

	void BuildGrid();
	void FitPackMesh();
	bool IsCellInGrid(const FIntPoint& Cell) const;

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

	// Keeps the pack readable in dark places, since the rig is otherwise lit by the world
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TObjectPtr<UPointLightComponent> DisplayLight;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "1"))
	int32 GridWidth = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "1"))
	int32 GridHeight = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.1", Units = cm))
	float CellSize = 4.0f;

	// From the camera to the grid
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "1.0", Units = cm))
	float GridDistance = 40.0f;

	// Instance custom data 0 is 1 on the hovered cell and 0 on the others
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid")
	TObjectPtr<UMaterialInterface> GridCellMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Grid", meta = (ClampMin = "0.01", Units = s))
	float GridScaleTime = 0.12f;

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
	FIntPoint HoveredCell = FIntPoint(INDEX_NONE, INDEX_NONE);
	float GridScale = 0.0f;
	float GridScaleTarget = 0.0f;
};
