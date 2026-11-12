#pragma once
// Shared default implementations for stateless modules (defined in registry.cpp).
#include "module_registry.hpp"
namespace plf {
void initOptionsDefault(void*) noexcept;
void destroyOptionsDefault(void*) noexcept;
void printModuleDefault(void*) noexcept;
// Shared renderer for "Key: value" lines (honors hideKeys / renames / keyColor / boldKeys).
void printKeyLine(FFContext& ctx, const std::string& rawKey, const std::string& value) noexcept;
bool jsonResultDefault(void*, nlohmann::json&) noexcept;
void jsonConfigDefault(void*, nlohmann::json&) noexcept;
} // namespace plf
