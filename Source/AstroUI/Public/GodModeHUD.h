#pragma once
#include "CoreMinimal.h"
// God-mode time controls + teaching-mode facts panels. VR uses world-space
// diegetic panels with laser-pointer interaction, not screen-space UMG.
// See CLAUDE.md Phase 11.

class ASTROUI_API FGodModeHUD
{
public:
    void BindToTimeController();
};
