#include "action_transition.h"
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
using genesis::ActionTransition;
namespace {
void expect(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        // Exhaust the ordinary 004105b0 gates, with its action-specific
        // earlier handler returning false as in the isolated original audit.
        for (bool locked : {false, true})
            for (bool blending : {false, true})
                for (auto requested : {9u, 34u, 35u}) {
                    ActionTransition state;
                    state.current = 34;
                    state.pending = 999;
                    state.frame = 12;
                    state.phase = 2;
                    state.blending = blending;
                    bool accepted = state.request(requested, locked);
                    expect(accepted ==
                               (!blending && requested != 34 && (!locked || requested == 9)),
                           "Recovered action-request gates");
                    if (accepted)
                        expect(state.current == 34 && state.pending == requested &&
                                   state.frame == 12 && state.phase == 0 && state.blending,
                               "Accepted request state");
                    else
                        expect(state.current == 34 && state.pending == 999 && state.phase == 2 &&
                                   state.blending == blending,
                               "Rejected request mutated state");
                }
        ActionTransition handled;
        handled.current = 34;
        handled.pending = 999;
        handled.phase = 2;
        expect(handled.request(35, false, true) && handled.current == 34 &&
                   handled.pending == 999 && handled.phase == 2 && !handled.blending,
               "Earlier special handler was treated as a new blend");
        expect(!handled.request(35, true, true), "Lock was bypassed before the special handler");
        struct Case {
            float phase, delta;
            bool commit;
        };
        // Original 0041b3a6 boundary cases: equality stays blending. The
        // float delta 1/30 is slightly above exact 1/30, so 4 + delta*30
        // commits even though its stored float phase rounds back to 5.
        const Case cases[] = {{0, 0, false},       {4, 0, false},          {5, 0, false},
                              {4, 1.f / 30, true}, {4.5f, 1.f / 60, true}, {5, 1.f / 144, true},
                              {5.5f, 0, true},     {0, .1f, false},        {0, .25f, true}};
        for (auto test : cases) {
            ActionTransition state;
            state.current = 34;
            state.pending = 35;
            state.frame = 12;
            state.phase = test.phase;
            state.blending = true;
            expect(state.advance_blend(test.delta) == test.commit,
                   "Original transition commit boundary");
            expect(state.current == (test.commit ? 35u : 34u) &&
                       state.frame == (test.commit ? 0.f : 12.f) && state.blending != test.commit,
                   "Original committed/frozen-source state");
        }
        ActionTransition state;
        state.current = 34;
        state.frame = 12;
        expect(state.request(35), "First blend request");
        expect(!state.advance_blend(.1f) && !state.request(36), "Overlapping blend request");
        expect(state.advance_blend(.1f) && state.current == 35 && state.request(36),
               "Next action after completed blend");
        std::puts("Native action request gates/five-phase transition clock passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
