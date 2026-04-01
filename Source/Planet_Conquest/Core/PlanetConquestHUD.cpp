// Copyright Benjamin Ramsell. All Rights Reserved.

#include "PlanetConquestHUD.h"
#include "Engine/Canvas.h"

void APlanetConquestHUD::DrawHUD()
{
	Super::DrawHUD();
	
	// Draw selection box if enabled
	if (bDrawSelectionBox && Canvas)
	{
		// Calculate box dimensions
		FVector2D BoxMin(FMath::Min(SelectionBoxStart.X, SelectionBoxEnd.X), FMath::Min(SelectionBoxStart.Y, SelectionBoxEnd.Y));
		FVector2D BoxMax(FMath::Max(SelectionBoxStart.X, SelectionBoxEnd.X), FMath::Max(SelectionBoxStart.Y, SelectionBoxEnd.Y));
		FVector2D BoxSize = BoxMax - BoxMin;
		
		// Draw semi-transparent fill
		FCanvasTileItem TileItem(BoxMin, BoxSize, FLinearColor(0.0f, 1.0f, 0.0f, 0.15f));
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(TileItem);
		
		// Draw border
		FCanvasBoxItem BoxItem(BoxMin, BoxSize);
		BoxItem.SetColor(FLinearColor::Green);
		BoxItem.LineThickness = 2.0f;
		Canvas->DrawItem(BoxItem);
	}
}
