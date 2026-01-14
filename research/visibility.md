# Visibility and Linkage

## Platform behavior
- MSVC class-level dllexport: exports vtable, typeinfo, ALL members (including private)
- GCC/Clang visibility("default") on class: exports vtable and typeinfo only
- MinGW: GCC frontend (key function rule) + Windows ABI (needs dllexport)

## vtable requirements
- Construction/destruction requires vtable (vptr initialization)
- dynamic_cast/typeid require typeinfo
- Polymorphic class crossing DLL boundary: MUST export vtable

## Strategy by class type
- Non-polymorphic: per-member export (minimizes symbols)
- Polymorphic, DLL boundary: class-level export (MSVC has no alternative)
- Polymorphic, header-only: BOOST_SYMBOL_VISIBLE (typeinfo consistency, not export)

## Pitfalls
- Class dllexport + private member with incomplete type = link error
- Missing key function export on GCC = "undefined reference to vtable"
- Implicit special members not exported unless class-level export used
