#include "st/core/project_revision.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

namespace {

int failures = 0;

void check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    using namespace st::core;

    static_assert(sizeof(ProjectRevision) == sizeof(std::uint64_t));
    static_assert(!std::is_default_constructible_v<ProjectRevision>);
    static_assert(!std::is_convertible_v<std::uint64_t, ProjectRevision>);

    const auto initial = ProjectRevision::initial();
    check(initial.value() == 0U, "initial revision is zero");

    const auto next = initial.next();
    check(next.has_value(), "initial revision can advance");
    check(next->value() == 1U, "next revision increments exactly once");
    check(initial.value() == 0U, "revision advancement does not mutate the original value");

    const auto restored = ProjectRevision::from_persisted(42U);
    check(restored.value() == 42U, "persisted revision preserves exact value");
    const auto restored_next = restored.next();
    check(restored_next.has_value() && restored_next->value() == 43U, "persisted revision advances deterministically");

    const auto maximum = ProjectRevision::from_persisted(
        std::numeric_limits<ProjectRevision::Value>::max());
    check(!maximum.next().has_value(), "maximum revision fails closed instead of wrapping");
    check(maximum.value() == std::numeric_limits<ProjectRevision::Value>::max(), "overflow failure leaves original revision unchanged");

    const auto project_a = ProjectId::parse("00000000000000000000000000000001");
    const auto project_b = ProjectId::parse("00000000000000000000000000000002");
    check(project_a && project_b, "project fixtures parse");

    if (project_a && project_b) {
        const ProjectSnapshotToken token{*project_a.value, ProjectRevision::from_persisted(7U)};
        check(token.matches(*project_a.value, ProjectRevision::from_persisted(7U)), "matching project and revision is current");
        check(!token.matches(*project_a.value, ProjectRevision::from_persisted(8U)), "revision change makes token stale");
        check(!token.matches(*project_b.value, ProjectRevision::from_persisted(7U)), "same revision under another project is stale");
        check(!token.matches(*project_b.value, ProjectRevision::from_persisted(8U)), "different project and revision is stale");

        const ProjectSnapshotToken same{*project_a.value, ProjectRevision::from_persisted(7U)};
        const ProjectSnapshotToken changed{*project_a.value, ProjectRevision::from_persisted(8U)};
        check(token == same, "snapshot token equality is exact project plus revision equality");
        check(!(token == changed), "snapshot token equality detects revision mismatch");
    }

    ProjectRevision current = ProjectRevision::initial();
    for (std::uint64_t expected = 1U; expected <= 10000U; ++expected) {
        const auto advanced = current.next();
        check(advanced.has_value(), "bounded repeated revision advance succeeds");
        if (!advanced) {
            break;
        }
        current = *advanced;
        check(current.value() == expected, "repeated revision advance is deterministic");
    }

    return failures == 0 ? 0 : 1;
}
