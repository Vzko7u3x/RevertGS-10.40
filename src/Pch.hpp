#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <detours.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "SDK.hpp"

#include "Flavor.hpp"
#include "Config.hpp"
#include "Image.hpp"
#include "Log.hpp"
#include "Scene.hpp"
