# Documenting a C++ Library

A step-by-step process for creating comprehensive user-facing documentation.

## Prerequisites

Before beginning, you MUST have:

1. **A documentation rule file** - Defines style, tone, formatting conventions, and code example standards. If the user has not provided this, **ask for it immediately**. Do not proceed without it.

2. **Access to the library source** - The `include/` directory, README, examples, and build scripts.

3. **Documentation format determined**:
   - If the library already has documentation, use its existing format and tooling
   - Otherwise, ask the user for their preferred format
   - **Default for Boost libraries:** Antora (user guides) + Mr. Docs (API reference)
     - See `capy/doc/` for reference implementation:
       - `antora.yml` - Antora component descriptor
       - `modules/ROOT/pages/` - AsciiDoc content
       - `mrdocs.yml` - Mr. Docs configuration
       - `local-playbook.yml` - Local build playbook

---

## Step 1: Discover Public Interfaces

Analyze the directory and file structure to understand the scope of what needs to be documented. This inventory is for planning purposes only—it does not appear in the final documentation.

### Location

For Boost libraries, public headers are located in `./include/boost/<library_name>/`.

### Exclusions

The following do NOT contain public interfaces:

- **Directories named `detail/` or `impl/`** - Implementation details, not public API
- **Symbols in `detail` namespaces** - Even within public headers, any symbol in a `detail` namespace is not part of the public API

### Process

1. List all `.hpp` files under `./include/boost/<library_name>/`
2. Exclude files in `detail/` and `impl/` subdirectories
3. For remaining files, identify public symbols (classes, functions, type aliases)
4. Exclude any symbols within `detail` namespaces

### Fallback Strategies

If the standard location is unclear or the library structure is non-standard:

1. **Check the README** - Look for `#include` lines in usage examples
2. **Check the examples directory** - Examine `#include` statements in example code
3. **Ask the user** - As a last resort, request clarification on where public headers are located

This produces a working inventory of public API surface area to guide subsequent documentation steps.

## Step 2: Generate the Introduction Page

Create `intro.adoc` (or `index.adoc`)—a single-page summary that orients the reader.

### Inputs to Consider

- **Public symbols and their Javadocs** - What does the API surface reveal about purpose?
- **Symbol names as context clues** - e.g., `unordered_map` suggests hash container semantics
- **README** - Often contains the "elevator pitch" and basic usage
- **Build scripts** (CMakeLists.txt, Jamfile) - Reveals dependencies and compiler requirements

### Required Sections

Following the pattern in [capy/doc/modules/ROOT/pages/index.adoc](capy/doc/modules/ROOT/pages/index.adoc):

1. **Title and one-sentence summary** - What the library is
2. **What This Library Does** - Core capabilities, bullet points
3. **What This Library Does Not Do** - Clarify scope boundaries
4. **Design Philosophy** - Key principles guiding the design
5. **Requirements** - C++ standard, compiler versions, dependencies
6. **Quick Example** - Minimal working code snippet
7. **Next Steps** - Links to quick-start, tutorials, reference

### Output

A single `.adoc` file that gives a developer everything needed to decide if this library fits their needs, and where to go next.

## Step 3: Determine Top-Level Sections

Analyze the library's domains and functionality to create a logical documentation structure.

### Process

1. **Identify functional units** - What distinct capabilities does the library offer?
2. **Generate multiple partitioning options** - Create 2-4 different ways to group the symbols
3. **Evaluate each option** against documentation principles:
   - Does each section have a clear, singular focus?
   - Can a reader skip sections irrelevant to their task?
   - Does the grouping match how users think about the problem domain?
   - Are dependencies between sections minimized?
4. **Select the best partitioning** or offer the user choices if unclear

### Section Count Guidelines

| Library Type | Sections | Example |
|--------------|----------|---------|
| Single container/utility | 1 | A specialized allocator |
| Focused library | 2-3 | JSON: value, parsing, serializing |
| Mid-size library | 3-5 | HTTP: messages, client, server, parsing, WebSocket |
| Large framework | 5-9+ | Networking: core, TCP, UDP, SSL, HTTP, WebSocket, utilities |

### Example: JSON Library Partitioning

**Option A:** By data flow
- Values (the container)
- Parsing (text → values)
- Serializing (values → text)

**Option B:** By user task
- Reading JSON
- Writing JSON
- Manipulating JSON

**Option C:** By abstraction level
- Low-level (SAX-style streaming)
- High-level (DOM-style trees)

Evaluate which option best matches how users approach the library.

### Fallback

If the optimal sectioning is unclear after analysis, present 2-3 options to the user with brief rationale for each and ask them to choose.

## Step 4: Light Fill Each Section

Populate each section with initial content following the documentation rule file. Complete the light fill for ALL sections before proceeding to the next step.

### Input

An externally provided documentation rule file that defines style, tone, and formatting conventions.

### Section Structure

Each section should follow this arc:

1. **Opening: Set expectations**
   - What will the reader be able to do after reading this section?
   - Frame the value proposition upfront

2. **Start simple**
   - Begin with the most common, relatable use case
   - Always include working code
   - Explain what the code does and why

3. **Build progressively**
   - Move from simple → complex
   - Each example builds on prior knowledge
   - Call out anything non-obvious or surprising
   - Stay within the section's defined boundaries

4. **Conclusion**
   - Summarize what was covered
   - Create a natural bridge to the next section

### Code Example Requirements

- Every concept must have accompanying code
- Examples should be compilable (not snippets with `...`)
- Show realistic use, not contrived demos
- Include brief explanation of what the code accomplishes

### Light Fill Scope

"Light fill" means:
- Establish the narrative flow
- Include key examples (can be refined later)
- Cover main concepts without exhaustive detail
- Mark areas needing expansion with `// TODO: expand` or similar

Complete all sections at this level before moving to Step 5.

## Step 5: Full Fill Each Section

Revisit each section to expand the light fill into comprehensive documentation.

### Expansion Tasks

1. **Add examples** - More code samples covering edge cases and variations
2. **Verify progression** - Does simple → complex flow make sense?
3. **Identify gaps** - What's missing for complete topic coverage?
4. **Check dependencies** - Ensure nothing is referenced before it's introduced

### Ordering Principle

**Introduce before referencing.** If a code snippet uses a type or function, that type/function must be explained earlier in the section (or in a prior section).

- Avoid forward references where possible
- If complexity requires referencing something not yet covered, use one of:
  - Brief inline explanation: "(`error_code`, which we'll cover in detail later)"
  - Link to the relevant section
  - Restructure to introduce the dependency first

### Section Review Checklist

After completing each section, verify:

- [ ] **Flow** - Does the narrative read smoothly from start to finish?
- [ ] **Code accuracy** - Do all snippets compile? Are they correct?
- [ ] **Domain coverage** - Is every relevant concept in this section addressed?
- [ ] **Completeness** - Better to include slightly more than leave something out

### Creative Solutions for Complex Topics

Some subjects resist linear presentation. Options:
- Split into sub-sections with clear boundaries
- Use a "preview" pattern: show simple usage first, explain internals later
- Provide a conceptual overview before diving into details
- Use diagrams to establish mental models before code

## Step 6: Add Supplementary Reference Pages

Create dedicated pages for material that supports the main sections but doesn't fit within their narrative flow.

### Page Types

1. **Concepts**
   - C++20 concepts used by the library
   - Semantic requirements and constraints
   - Valid expressions and their guarantees

2. **Terminology / Glossary**
   - Domain-specific terms needing dedicated explanation
   - Terms not adequately covered by Javadoc API reference
   - Definitions that would clutter the main narrative

3. **Design Rationale**
   - Extract from design notes, context documents, or commit history
   - Explain *why* certain decisions were made
   - Trade-offs considered and rejected alternatives

4. **References**
   - Academic papers the design draws from
   - Related standards (ISO, RFCs, etc.)
   - Other libraries or implementations worth comparing

### Organization

Group supplementary pages into logical sections:

```
doc/
  modules/ROOT/pages/
    concepts/
      concept_name.adoc
      ...
    reference/
      glossary.adoc
      design-rationale.adoc
      bibliography.adoc
```

### Guidelines

- Each page should be self-contained and linkable
- Use consistent formatting across all supplementary pages
- Cross-link from main sections where terms/concepts appear
- Keep design rationale factual, not defensive

## Step 7: Final Review

Perform a complete review of the generated documentation from first page to last.

### Prerequisites

**A documentation rule file MUST be provided by the user.** If you have not received a documentation rule:

> ⚠️ **STOP.** Ask the user for the documentation rule file before proceeding. Do not perform any documentation operations without this input.

### Review Process

1. **Read through sequentially** - Start at intro, end at last supplementary page
2. **Check alignment with goals**
   - Does the documentation achieve what was outlined in the intro?
   - Can a new user get started?
   - Can an experienced user find reference material?
3. **Verify compliance with documentation rule**
   - Style and tone
   - Formatting conventions
   - Required sections present
   - Code example standards met

### Final Checklist

- [ ] Introduction accurately represents the library
- [ ] All top-level sections are complete
- [ ] Progression from simple → complex is maintained throughout
- [ ] No forward references to unexplained concepts
- [ ] All code examples are accurate and compilable
- [ ] Supplementary pages are linked where relevant
- [ ] Terminology is consistent across all pages
- [ ] Navigation structure is logical and complete
