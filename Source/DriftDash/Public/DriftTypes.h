// DriftDash - shared constants.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/** Drivable surfaces: road, sidewalks, ramps (Config/DefaultEngine.ini, "DriftGround"). The car's body ignores them
 *  and follows them with ray casts; walls (buildings, ramp backs) are WorldStatic and block the body. */
#define ECC_DriftGround ECC_GameTraceChannel1

DRIFTDASH_API DECLARE_LOG_CATEGORY_EXTERN(LogDrift, Log, All);
