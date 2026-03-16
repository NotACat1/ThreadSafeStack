#include "ModernThreadSafeStack.h"

// Explicit template instantiations for commonly used types.
// This forces the compiler to generate implementations for these types
// in this translation unit, which can reduce compilation times and
// prevent linker errors when the template is used across multiple files.
template class ModernThreadSafeStack<int>;
template class ModernThreadSafeStack<double>;
template class ModernThreadSafeStack<std::string>;