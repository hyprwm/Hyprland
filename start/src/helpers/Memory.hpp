#pragma once

#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/memory/Atomic.hpp>

using namespace Hyprutils::Memory;

template <typename T>
using SP = Hyprutils::Memory::CSharedPointer<T>;
template <typename T>
using WP = Hyprutils::Memory::CWeakPointer<T>;
template <typename T>
using UP = Hyprutils::Memory::CUniquePointer<T>;
template <typename T>
using ASP = Hyprutils::Memory::CAtomicSharedPointer<T>;
