#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/memory/Atomic.hpp>

using namespace Hyprutils::Memory;

#define SP CSharedPointer
#define WP CWeakPointer
#define UP CUniquePointer
