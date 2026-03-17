#include "WaitingThreadSafeStack.h"

// Explicit template instantiations for commonly used types.
// This forces the compiler to generate implementations for these types
// in this translation unit. This approach helps reduce compilation times
// and avoids potential linker issues when the template is used across
// multiple translation units.
template class WaitingThreadSafeStack<int>;
template class WaitingThreadSafeStack<double>;
template class WaitingThreadSafeStack<std::string>;