#pragma once

#ifndef __has_cpp_attribute
  #define kiln_lifetime_capture_by(x)
#elif __has_cpp_attribute(lifetime_capture_by)
  #define kiln_lifetime_capture_by(x) [[lifetime_capture_by(x)]]
#elif __has_cpp_attribute(msvc::lifetime_capture_by)
  #define kiln_lifetime_capture_by(x) [[msvc::lifetime_capture_by(x)]]
#elif __has_cpp_attribute(clang::lifetime_capture_by)
  #define kiln_lifetime_capture_by(x) [[clang::lifetime_capture_by(x)]]
#else
  #define kiln_lifetime_capture_by(x)
#endif

#ifndef __has_cpp_attribute
  #define kiln_lifetime_capture_by_this
#elif __has_cpp_attribute(lifetime_capture_by_this)
  #define kiln_lifetime_capture_by_this [[lifetime_capture_by_this]]
#elif __has_cpp_attribute(msvc::lifetime_capture_by_this)
  #define kiln_lifetime_capture_by_this [[msvc::lifetime_capture_by_this]]
#elif __has_cpp_attribute(clang::lifetime_capture_by_this)
  #define kiln_lifetime_capture_by_this [[clang::lifetime_capture_by_this]]
#else
  #define kiln_lifetime_capture_by_this
#endif
