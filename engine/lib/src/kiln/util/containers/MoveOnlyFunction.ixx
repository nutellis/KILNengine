module;

#include <cstddef>

export module kiln.util.containers.MoveOnlyFunction;

import kiln.util.concepts.function;
import kiln.util.containers.Function;

namespace kiln::util {

export using util::storable_in_function_c;
export using util::decays_to_storable_in_function_c;

export template <
    function_c  Signature_T,
    std::size_t size_T      = default_function_size(),
    std::size_t alignment_T = default_function_alignment()>
using MoveOnlyFunction = Function<Signature_T, true, size_T, alignment_T>;

export using util::any_cast;

}   // namespace kiln::util
