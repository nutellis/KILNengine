#pragma once

#ifndef __has_cpp_attribute
  #define kiln_lifetimebound
#elif __has_cpp_attribute(lifetimebound)
  #define kiln_lifetimebound [[lifetimebound]]
#elif __has_cpp_attribute(msvc::lifetimebound)
  #define kiln_lifetimebound [[msvc::lifetimebound]]
#elif __has_cpp_attribute(clang::lifetimebound)
  #define kiln_lifetimebound [[clang::lifetimebound]]
#else
  #define kiln_lifetimebound
#endif
