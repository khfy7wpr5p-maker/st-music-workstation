#include "st/core/event_projection_mutation.hpp"

#include <type_traits>

int main()
{
    using namespace st::core;

    static_assert(!std::is_aggregate_v<PreparedEventProjectionLinkAddition>);
    static_assert(!std::is_default_constructible_v<PreparedEventProjectionLinkAddition>);
    static_assert(!std::is_constructible_v<
        PreparedEventProjectionLinkAddition,
        ProjectSnapshotToken,
        ProjectRevision,
        EventProjectionLinkCandidate>);

    static_assert(std::is_copy_constructible_v<PreparedEventProjectionLinkAddition>);
    static_assert(!std::is_copy_assignable_v<PreparedEventProjectionLinkAddition>);
    static_assert(!std::is_move_assignable_v<PreparedEventProjectionLinkAddition>);

    return 0;
}
