#pragma once

#include <hyprutils/memory/WeakPtr.hpp>

using namespace Hyprutils::Memory;

template <typename T>
using SP = Hyprutils::Memory::CSharedPointer<T>;
template <typename T>
using WP = Hyprutils::Memory::CWeakPointer<T>;
template <typename T>
using UP = Hyprutils::Memory::CUniquePointer<T>;
