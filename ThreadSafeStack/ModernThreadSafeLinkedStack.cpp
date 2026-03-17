#include "ModernThreadSafeLinkedStack.h"
#include <string>

// Explicit template instantiations for commonly used types.
// This forces the compiler to generate implementations for these types
// in this translation unit, which can reduce compilation times and
// prevent linker errors when the template is used across multiple files.
template class ModernThreadSafeLinkedStack<int>;
template class ModernThreadSafeLinkedStack<double>;
template class ModernThreadSafeLinkedStack<std::string>;