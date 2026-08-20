#include "st/core/event_projection_publication.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <typename Id>
Id require_id(std::string_view text)
{
    const auto parsed = Id::parse(text);
    if (!parsed) {
        std::cerr << "FAIL: fixture ID did not parse: " << text << '\n';
        ++failures;
        return *Id::parse("00000000000000000000000000000001").value;
    }
    return *parsed.value;
}

class TestValidationView final : public st::core::EventProjectionValidationView {
public:
    explicit TestValidationView(st::core::ProjectId project_id)
        : project_id_(std::move(project_id))
    {
    }

    [[nodiscard]] st::core::ProjectId project_id() const noexcept override
    {
        ++project_id_calls_;
        return project_id_;
    }

    [[nodiscard]] bool contains_event(
        const st::core::ScopedMusicalEventId& id) const noexcept override
    {
        ++event_calls_;
        return contains(events_, id);
    }

    [[nodiscard]] bool contains_score(
        const st::core::ScopedScoreEntityId& id) const noexcept override
    {
        ++projection_calls_;
        return contains(scores_, id);
    }

    [[nodiscard]] bool contains_midi(
        const st::core::ScopedMidiEntityId&) const noexcept override
    {
        ++projection_calls_;
        return false;
    }

    [[nodiscard]] bool contains_tab(
        const st::core::ScopedTabEntityId&) const noexcept override
    {
        ++projection_calls_;
        return false;
    }

    [[nodiscard]] bool contains_audio(
        const st::core::ScopedAudioEntityId&) const noexcept override
    {
        ++projection_calls_;
        return false;
    }

    [[nodiscard]] bool contains_link(
        const st::core::EventProjectionLinkCandidate& link) const noexcept override
    {
        ++link_calls_;
        return contains(links_, link);
    }

    void set_project_id(st::core::ProjectId project_id) noexcept
    {
        project_id_ = std::move(project_id);
    }

    void reset_calls() const noexcept
    {
        project_id_calls_ = 0U;
        event_calls_ = 0U;
        projection_calls_ = 0U;
        link_calls_ = 0U;
    }

    [[nodiscard]] std::size_t project_id_calls() const noexcept
    {
        return project_id_calls_;
    }

    [[nodiscard]] std::size_t relation_calls() const noexcept
    {
        return event_calls_ + projection_calls_ + link_calls_;
    }

    std::vector<st::core::ScopedMusicalEventId> events_;
    std::vector<st::core::ScopedScoreEntityId> scores_;
    std::vector<st::core::EventProjectionLinkCandidate> links_;

private:
    template <typename T>
    [[nodiscard]] static bool contains(
        const std::vector<T>& values,
        const T& value) noexcept
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    st::core::ProjectId project_id_;
    mutable std::size_t project_id_calls_{0U};
    mutable std::size_t event_calls_{0U};
    mutable std::size_t projection_calls_{0U};
    mutable std::size_t link_calls_{0U};
};

} // namespace

int main()
{
    using namespace st::core;

    static_assert(!std::is_aggregate_v<RevalidatedEventProjectionLinkAddition>);
    static_assert(!std::is_default_constructible_v<RevalidatedEventProjectionLinkAddition>);
    static_assert(!std::is_constructible_v<
        RevalidatedEventProjectionLinkAddition,
        ProjectSnapshotToken,
        ProjectSnapshotToken,
        EventProjectionLinkCandidate>);
    static_assert(std::is_copy_constructible_v<RevalidatedEventProjectionLinkAddition>);
    static_assert(!std::is_copy_assignable_v<RevalidatedEventProjectionLinkAddition>);
    static_assert(!std::is_move_assignable_v<RevalidatedEventProjectionLinkAddition>);

    const auto project_a = require_id<ProjectId>("00000000000000000000000000000001");
    const auto project_b = require_id<ProjectId>("00000000000000000000000000000002");
    const ScopedMusicalEventId event_a{
        project_a,
        require_id<MusicalEventId>("00000000000000000000000000000011")};
    const ScopedScoreEntityId score_a{
        project_a,
        require_id<ScoreEntityId>("00000000000000000000000000000021")};
    const EventProjectionLinkCandidate link{event_a, score_a};
    const auto revision = ProjectRevision::from_persisted(7U);

    TestValidationView view{project_a};
    view.events_.push_back(event_a);
    view.scores_.push_back(score_a);

    const auto prepared_result = prepare_event_projection_link_addition(
        link,
        view,
        revision,
        revision);
    check(static_cast<bool>(prepared_result), "fixture preparation succeeds");
    if (!prepared_result) {
        return 1;
    }
    const auto prepared = *prepared_result.value;

    {
        view.reset_calls();
        const auto result = revalidate_prepared_event_projection_link_addition(
            prepared,
            view,
            revision);
        check(static_cast<bool>(result), "current prepared plan revalidates");
        check(result.error == EventProjectionPublicationRevalidationError::none, "successful revalidation has no error");
        check(result.value->base_snapshot.matches(project_a, revision), "base snapshot matches current Project state");
        check(result.value->next_snapshot.matches(project_a, ProjectRevision::from_persisted(8U)), "next snapshot is exact current plus one");
        check(result.value->link == link, "exact prepared link is preserved");
        check(view.project_id_calls() == 1U, "publication revalidation reads current ProjectId exactly once");
    }

    {
        view.reset_calls();
        const auto result = revalidate_prepared_event_projection_link_addition(
            prepared,
            view,
            ProjectRevision::from_persisted(8U));
        check(!result, "stale prepared revision is rejected");
        check(result.error == EventProjectionPublicationRevalidationError::stale_prepared_revision, "stale prepared revision has explicit error");
        check(view.project_id_calls() == 1U, "stale check reads current ProjectId once");
        check(view.relation_calls() == 0U, "stale prepared revision rejects before relation lookup");
    }

    {
        view.set_project_id(project_b);
        view.reset_calls();
        const auto result = revalidate_prepared_event_projection_link_addition(
            prepared,
            view,
            revision);
        check(!result, "prepared plan from another Project is rejected");
        check(result.error == EventProjectionPublicationRevalidationError::prepared_project_mismatch, "Project mismatch is explicit");
        check(view.project_id_calls() == 1U, "Project mismatch reads current ProjectId once");
        check(view.relation_calls() == 0U, "Project mismatch rejects before relation lookup");
        view.set_project_id(project_a);
    }

    {
        view.links_.push_back(link);
        view.reset_calls();
        const auto result = revalidate_prepared_event_projection_link_addition(
            prepared,
            view,
            revision);
        check(!result, "duplicate introduced after preparation is rejected");
        check(result.error == EventProjectionPublicationRevalidationError::duplicate_link, "duplicate revalidation failure is preserved");
        check(view.project_id_calls() == 1U, "duplicate revalidation still pins ProjectId once");
        view.links_.clear();
    }

    {
        view.scores_.clear();
        view.reset_calls();
        const auto result = revalidate_prepared_event_projection_link_addition(
            prepared,
            view,
            revision);
        check(!result, "missing endpoint introduced after preparation is rejected");
        check(result.error == EventProjectionPublicationRevalidationError::projection_missing, "missing endpoint revalidation failure is preserved");
        view.scores_.push_back(score_a);
    }

    for (int iteration = 0; iteration < 10000; ++iteration) {
        const auto repeated = revalidate_prepared_event_projection_link_addition(
            prepared,
            view,
            revision);
        check(static_cast<bool>(repeated), "repeated publication revalidation remains successful");
        check(repeated.value->next_snapshot.revision().value() == 8U, "repeated publication revalidation remains deterministic");
    }

    return failures == 0 ? 0 : 1;
}
