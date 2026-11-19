#pragma once
namespace bsf {
/// True when mimalloc serves malloc, operator new and allocations made inside system libraries.
/// Prints a one-line report to stderr when `print` is set or when the override is not active.
bool mimalloc_active(bool print);
}  // namespace bsf
