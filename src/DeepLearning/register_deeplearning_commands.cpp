/**
 * @file register_deeplearning_commands.cpp
 * @brief Registers DeepLearning-owned commands with the CommandRegistry singleton
 */

 #include "DeepLearning/register_deeplearning_commands.hpp"

 #include "Commands/Core/CommandRegistry.hpp"
 
 namespace dl {
 
 void register_deeplearning_commands() {
     auto & reg = commands::CommandRegistry::instance();
 }
 
 }// namespace dl
 