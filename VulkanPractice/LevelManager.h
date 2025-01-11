#pragma once
#ifndef LEVEL_MANAGER_H
#define LEVEL_MANAGER_H

#include "common.h"
#include "ResourceManager.h"
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xform.h>


class LevelManager {
public:
	pxr::UsdStageRefPtr testPointer;

	void testImport();
};
#endif // !LEVEL_MANAGER_H
