export module kiln.reg.EntryTraits;

import kiln.reg.entry_c;

namespace kiln::reg {

/*
 * Customization points:
 *      - `static auto describe_build(BuildDirector<Entry_T> build_director) -> void`
 *      - `constexpr static bool is_configuration_entry`
 */
export template <entry_c Entry_T>
struct EntryTraits;

}   // namespace kiln::reg
