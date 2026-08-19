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

    const auto matching_transition = prepare_revision_advance(
        ProjectRevision::from_persisted(42U),
        ProjectRevision::from_persisted(42U));
    check(static_cast<bool>(matching_transition), "matching expected revision prepares advancement");
    check(matching_transition.error == RevisionAdvanceError::none, "matching expected revision has no error");
    check(matching_transition.next_revision->value() == 43U, "matching expected revision yields exactly current plus one");

    const auto stale_transition = prepare_revision_advance(
        ProjectRevision::from_persisted(42U),
        ProjectRevision::from_persisted(41U));
    check(!stale_transition, "stale expected revision is rejected");
    check(stale_transition.error == RevisionAdvanceError::stale_expected_revision, "stale rejection is explicit");
    check(!stale_transition.next_revision.has_value(), "stale rejection publishes no next revision");

    const auto maximum = ProjectRevision::from_persisted(
        std::numeric_limits<ProjectRevision::Value>::max());
    check(!maximum.next().has_value(), "maximum revision fails closed instead of wrapping");
    check(maximum.value() == std::numeric_limits<ProjectRevision::Value>::max(), "overflow failure leaves original revision unchanged");

    const auto overflow_transition = prepare_revision_advance(maximum, maximum);
    check(!overflow_transition, "matching terminal revision cannot advance");
    check(overflow_transition.error == RevisionAdvanceError::overflow, "terminal matching revision reports overflow");
    check(!overflow_transition.next_revision.has_value(), "overflow publishes no next revision");

    const auto stale_at_maximum = prepare_revision_advance(
        maximum,
        ProjectRevision::from_persisted(std::numeric_limits<ProjectRevision::Value>::max() - 1U));
    check(!stale_at_maximum, "stale expected revision at terminal current value is rejected");
    check(stale_at_maximum.error == RevisionAdvanceError::stale_expected_revision, "stale precondition takes deterministic precedence over overflow");

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
    for (std::uint64_t expected_value = 1U; expected_value <= 10000U; ++expected_value) {
        const auto advanced = prepare_revision_advance(current, current);
        check(static_cast<bool>(advanced), "bounded repeated revision transition succeeds");
        if (!advanced) {
            break;
        }
        current = *advanced.next_revision;
        check(current.value() == expected_value, "repeated revision transition is deterministic");
    }

    return failures == 0 ? 0 : 1;
}
