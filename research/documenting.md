# Documentation Style Rules

## Part 1: Grammar & English

- Once you name something, never rename it mid-document—readers assume synonyms signal a subtle distinction
- Replace "it", "this", "that" with explicit nouns when ambiguous
- Define acronyms only when significantly shorter AND appearing many times; otherwise use the full term
- Clarify who performs an action—especially whether a function triggers automatically or the user must call it
- Aim for 15–20 words per sentence
- "That" for essential clauses (no comma); "which" for nonessential (comma before)—US English
- Bulleted lists = unordered; numbered lists = ordered (sequence matters)
- Every list needs a lead-in sentence ending with a colon
- Sentences should chain logically—picking up the first should pull the rest in sequence
- Avoid idioms—invisible to native speakers but block international readers
- Link text must be clear out of context: "Read the style guide" not "Click here"

## Part 2: General Documentation Guidance

### Structure

- State scope AND non-scope (what the document deliberately excludes)
- Structure by complexity: simple/common first, exotic/advanced last
- Technical progression: each section builds on prior knowledge
- Concrete before abstract: features first, then formalizing concepts
- Avoid orphan subsections (single H3 under H2)—reorganize or add content
- Avoid H4+ nesting; convert to bulleted lists instead

### Page Organization

- Beefy but bounded pages: substantial content with cohesive focus
- Progressive flow across pages: each builds on the last, graded difficulty
- Enticing transitions: end of one page leads naturally to the next
- Upfront orientation: each page opens with 1-2 sentences stating what the reader will learn

### Content Priorities

- Document gotchas, caveats, and edge cases—these matter more than happy-path documentation:
  - Thread safety constraints
  - Ownership and lifetime issues
  - Platform-specific behavior
  - Performance cliffs
  - Non-obvious prerequisites
- Show when to use AND when NOT to use each feature
- Answer: What? Why? How?—in that order
- Visual aids where they clarify: diagrams for flow, state, timing
- Omit the irrelevant: cut what doesn't belong

### Diátaxis Document Types

Keep these separate; mixing them produces documentation that fails at everything:

| Type | Purpose | User State |
|------|---------|------------|
| **Tutorial** | Learning by doing | "I don't know what I don't know" |
| **How-To** | Accomplishing a goal | "I know what I need to do" |
| **Explanation** | Understanding why | "I want to understand" |
| **Reference** | Looking up details | "I need the exact specs" |

**Tutorials:** Minimal explanation. Immediate visible feedback. You are responsible for success. Link to explanations elsewhere—the tutorial isn't the place.

**How-Tos:** Problem-oriented titles ("How to configure X" not "X Configuration"). Assume baseline competence. Get to the solution quickly. Not exhaustive—link to reference for options.

**Explanations:** Answer "why?" and "how does this fit together?" Opinions allowed. Higher perspective—design decisions, trade-offs, alternatives considered.

**Reference:** Mirror code structure. Consistent format. Lookup tool, not reading material.

### Anti-Patterns

- Teaching in tutorials (should be *doing*, not *teaching*)
- Listing options in how-tos (reference lists options)
- Instructions in reference (reference describes; how-tos instruct)
- Opinion-free explanations (should say *why*, not just *what*)
- FAQ as primary organization (sign of failed documentation elsewhere)

### Evaluation

The 30-minute test: Can an unfamiliar user understand what it does (5 min), install it (5 min), and complete a basic task (20 min)?

## Part 3: C++ Specific Rules

### Namespace Conventions

Avoid repeating large namespace qualifiers; use unqualified names throughout.

Begin documentation with an admonition establishing assumed context:

> Code snippets throughout the documentation are written as if the following declarations are in effect:
> ```cpp
> #include <boost/buffers.hpp>
> using namespace boost::buffers;
> ```

### Code Examples

- Examples must compile and run
- Show complete, minimal programs when possible
- Include expected output where applicable
- Annotate with inline comments explaining non-obvious parts
- Test examples as part of CI

### Error Documentation

- What errors can occur?
- How are they reported (exceptions, error codes, callbacks)?
- What should users do when errors occur?
- Provide example error handling code

### Javadoc Conventions

- Do not show `@tparam` for variadic template args (`Args...`); only show `@param`
- Briefs for functions returning a value should start with "Return" (with few exceptions)