#pragma once

// Convenience include for node authors.
//
// Most node compile callbacks live in this same folder:
//   `src/core/registry/nodes/*.cpp`
//
// The actual helper implementations live in:
//   `src/core/compiler/nodeCompileHelpers.h`
//
// Keeping this wrapper here makes the “toolbox” easy to discover for new
// contributors: when you're editing a node file, the helpers are one include
// away.

#include "../../compiler/nodeCompileHelpers.h"
