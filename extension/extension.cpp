#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

#include "world_adapter.hpp"

namespace {

void initialize(godot::ModuleInitializationLevel level) {
  if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
    GDREGISTER_ABSTRACT_CLASS(murim_godot::WorldAdapter);
  }
}

void uninitialize(godot::ModuleInitializationLevel /*level*/) {}

}  // namespace

/// Entry symbol named in `murim.gdextension`; Godot calls it when it loads the
/// library.
extern "C" GDExtensionBool GDE_EXPORT
murim_init(GDExtensionInterfaceGetProcAddress get_proc_address,
           GDExtensionClassLibraryPtr library,
           GDExtensionInitialization* initialization) {
  godot::GDExtensionBinding::InitObject init_object(get_proc_address, library,
                                                    initialization);
  init_object.register_initializer(initialize);
  init_object.register_terminator(uninitialize);
  init_object.set_minimum_library_initialization_level(
      godot::MODULE_INITIALIZATION_LEVEL_SCENE);
  return init_object.init();
}
