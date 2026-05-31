---
name: create
description:
  Turn a vague idea into a concrete, actionable spec through structured
  dialogue — disambiguating requirements, surfacing decision points, and
  validating technical feasibility before any code is written.
---

# Create

Begin from the endpoint: what does the user ultimately want to exist when this
is done? If `$ARGUMENTS` is provided, use it as the starting point. Otherwise
ask: *"Tell me what you want to create."*

Then work backward from that endpoint toward the present. Everything that must
be true for the idea to reach its endpoint is a thread to pull. In each turn,
do two things:

1. **Surface ambiguities** — wherever the path from idea to endpoint could be
   interpreted more than one way, ask about it. Pursue the gaps that actually
   change what gets built, one cluster at a time, not every minor detail.

2. **Probe feasibility** — as the picture sharpens, flag anything that may be
   hard or impossible to build. Be specific about why, and offer a concrete
   alternative when something won't work.

Converge when the core mechanic is unambiguous and every known risk is on the
table — not when every conceivable question is answered. Then summarize the
aligned idea and ask for approval.

**Do not write any code until the user approves.** That restraint is the whole
point of this skill.
