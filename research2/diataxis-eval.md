# Diátaxis Evaluation Rules

A framework for evaluating documentation against Diátaxis principles. Each rule is a testable criterion. Documents that fail a rule should be revised according to the improvement suggestion.

---

## How to Use This Framework

1. **Identify the document type** using the Classification Test below
2. **Apply Universal Rules** (all documents must pass these)
3. **Apply Type-Specific Rules** for the identified document type
4. **Score**: Count passed rules vs. total applicable rules
5. **Improve**: For each failed rule, apply the suggested fix

---

## Classification Test

Before evaluating, determine what type of documentation you have:

| Question | Yes → | No → |
|----------|-------|------|
| Does it guide the user through actions/steps? | Go to Q2 | Go to Q3 |
| **Q2**: Is the user learning something new, or accomplishing a real task? | Learning → **Tutorial** | Real task → **How-to Guide** |
| **Q3**: Does it describe facts about the system, or explain why/how things work conceptually? | Facts → **Reference** | Concepts → **Explanation** |

**Quick heuristic**:
- Tutorial: "Let me teach you..."
- How-to: "Here's how to accomplish X..."
- Reference: "Here are the specifications..."
- Explanation: "Here's why this works the way it does..."

---

## Universal Rules (Apply to All Documentation)

These rules apply regardless of document type. Weight: **Critical**

### U1. Single Purpose
**Rule**: The document serves exactly one of the four documentation needs (learning, task completion, information lookup, or understanding).

**Test**: Can you summarize what the document does in one sentence that maps to exactly one quadrant?

**Fail indicators**: 
- Document tries to teach AND provide reference
- Document explains concepts while also walking through tasks
- Sections feel like they belong in different documents

**Improvement**: Split the document. Extract content that serves a different need into its own document of the appropriate type. Link between them.

---

### U2. No Type Blurring
**Rule**: The document does not blend characteristics of different documentation types.

**Test**: Read each paragraph. Does every paragraph serve the same user need?

**Fail indicators**:
- Tutorial contains extensive explanation digressions
- How-to guide teaches concepts instead of assuming competence
- Reference material includes step-by-step procedures
- Explanation includes task instructions

**Improvement**: Move misplaced content to documents of the appropriate type. Replace with brief statements and links: "For more on why this works, see [Explanation]. For step-by-step setup, see [How-to Guide]."

---

### U3. User Need Orientation
**Rule**: The document is structured around user needs, not product features or internal organization.

**Test**: Does the document's structure reflect what users need to accomplish, learn, look up, or understand—rather than mirroring code structure, team organization, or feature lists?

**Fail indicators**:
- Sections named after internal components users don't interact with
- Organization follows development timeline rather than user journey
- Content grouped by "what we built" rather than "what you need"

**Improvement**: Reorganize around user tasks, questions, or learning goals. Ask: "What is the user trying to do/learn/find/understand?" and structure accordingly.

---

### U4. Appropriate Linking
**Rule**: The document links to other documentation types rather than inlining their content.

**Test**: When the document needs to reference information from another quadrant, does it link rather than duplicate?

**Fail indicators**:
- Full explanations embedded in tutorials
- Complete reference tables inside how-to guides  
- Task procedures written out in explanation documents

**Improvement**: Replace inline content with: "[Brief one-sentence summary]. See [Document Name] for [details/steps/reference/explanation]."

---

## Tutorial Rules

Tutorials are **learning-oriented**. They help users acquire skills through guided experience.

### T1. Practical Activity (Weight: Critical)
**Rule**: The tutorial has the user DO things, not just read about things.

**Test**: Count the concrete actions the user performs. Is there at least one action every few paragraphs?

**Fail indicators**:
- Long stretches of text with no user action
- "Conceptual overview" sections before hands-on work
- User reads more than they do

**Improvement**: Convert explanatory passages into actions. Instead of "The database stores data in tables," write "Create your first table by running: [command]. You'll see [expected output]."

---

### T2. Visible Results (Weight: Critical)
**Rule**: Every action produces a visible, meaningful result that the user can verify.

**Test**: After each step, can the user see that something happened? Is there expected output or a verification step?

**Fail indicators**:
- Steps with no observable outcome
- "Trust that this worked" situations
- No expected output shown

**Improvement**: After each action, add: "You should see [specific output/result]. This confirms [what it means]." Include actual example output.

---

### T3. Achievable Goal (Weight: Critical)
**Rule**: The tutorial leads to a concrete accomplishment the user can be proud of.

**Test**: At the end, has the user built/created/accomplished something tangible?

**Fail indicators**:
- Tutorial ends without a "finished product"
- User has learned concepts but made nothing
- No sense of accomplishment

**Improvement**: Define the end goal upfront ("By the end, you'll have built X"). Ensure the tutorial culminates in that deliverable. Celebrate the achievement: "You've successfully created your first [X]!"

---

### T4. Reliable Path (Weight: Critical)
**Rule**: Following the tutorial exactly produces the promised results every time.

**Test**: Can a new user follow the tutorial verbatim and succeed without external help?

**Fail indicators**:
- Steps that require unstated prerequisites
- Commands that fail due to version differences
- Ambiguous instructions with multiple interpretations

**Improvement**: Test the tutorial with actual beginners. Pin versions. State all prerequisites explicitly. Remove all ambiguity from instructions.

---

### T5. Minimal Explanation (Weight: High)
**Rule**: Explanations are brief—just enough to make sense of the action, never more.

**Test**: For each explanation, ask: "Does the user need this NOW to continue?" If no, it's too much.

**Fail indicators**:
- Paragraphs explaining theory before practice
- Digressions into "why" when user needs "what"
- Explanations longer than a sentence or two

**Improvement**: Reduce explanations to one sentence maximum. Move deeper explanations to linked Explanation documents: "We use HTTPS for security. (See [Understanding HTTPS] for details.)"

---

### T6. Concrete and Specific (Weight: High)
**Rule**: Instructions reference specific, concrete things—not abstractions or generalizations.

**Test**: Are all instructions actionable without interpretation? Does the user know exactly what to type/click/do?

**Fail indicators**:
- "Configure the appropriate settings"
- "Use a suitable value"
- "Set up your environment" (without saying how)

**Improvement**: Replace vague instructions with exact ones: "Set `timeout` to `30`" not "Set an appropriate timeout value."

---

### T7. Single Path (Weight: High)
**Rule**: The tutorial presents one way to accomplish the goal, with no options or alternatives.

**Test**: Does the user ever have to make a choice about how to proceed?

**Fail indicators**:
- "You can either do A or B"
- "If you prefer, you could..."
- Multiple approaches presented

**Improvement**: Pick the best path and present only that. Save alternatives for how-to guides: "This tutorial uses [X]. For other approaches, see [How-to Guide]."

---

### T8. Expected Narrative (Weight: Medium)
**Rule**: The tutorial tells users what to expect before they see it.

**Test**: Before each significant output, does the tutorial prepare the user for what they'll see?

**Fail indicators**:
- Surprising outputs with no forewarning
- User unsure if output is correct
- No "you should see..." statements

**Improvement**: Before actions with significant output, add: "When you run this, you'll see [description]." After: "This output shows [interpretation]."

---

### T9. First-Person Plural (Weight: Medium)
**Rule**: The tutorial uses "we" language to establish partnership between teacher and learner.

**Test**: Does the tutorial say "we will" rather than "you will" or passive voice?

**Fail indicators**:
- "You should configure..."
- "The system will be configured..."
- Impersonal, distant tone

**Improvement**: Rewrite in first-person plural: "Let's configure..." / "We'll now set up..." / "Together, we'll build..."

---

### T10. States Prerequisites (Weight: Medium)
**Rule**: Everything the user needs before starting is explicitly listed upfront.

**Test**: Could someone with zero context know exactly what they need before beginning?

**Fail indicators**:
- Mid-tutorial surprises ("You'll need X installed")
- Assumed knowledge not stated
- Required accounts/tools discovered during tutorial

**Improvement**: Add a "Before you begin" section listing: required software/tools, accounts needed, time estimate, skill prerequisites.

---

## How-to Guide Rules

How-to guides are **goal-oriented**. They help competent users accomplish specific tasks.

### H1. Addresses Real Problem (Weight: Critical)
**Rule**: The guide solves a genuine problem or achieves a goal that users actually have.

**Test**: Would a real user search for this? Does it answer "How do I [specific thing]?"

**Fail indicators**:
- Guide describes feature mechanics, not user goals
- Title is feature-name, not task-name
- No clear problem being solved

**Improvement**: Reframe around user goals. "Configuring Widget Settings" → "How to optimize Widget for high-traffic sites." Ask: "What job is the user hiring this guide to do?"

---

### H2. Assumes Competence (Weight: Critical)
**Rule**: The guide assumes the user already knows the basics and can fill in minor gaps.

**Test**: Does the guide avoid teaching fundamentals? Does it assume familiarity with the domain?

**Fail indicators**:
- Explains basic concepts before the task
- Teaches vocabulary or foundational skills
- Treats user as beginner

**Improvement**: Remove teaching. Link to tutorials for prerequisites: "This guide assumes you've completed [Getting Started Tutorial]." Jump straight to the task.

---

### H3. Action-Focused (Weight: Critical)
**Rule**: The guide consists primarily of actions to take, not concepts to understand.

**Test**: Is more than 80% of the content actionable steps?

**Fail indicators**:
- Long explanatory sections
- Conceptual background before steps
- "Understanding X" sections within the guide

**Improvement**: Move explanations to Explanation documents. Reduce remaining explanations to single sentences. The guide should read as: do this, then this, then this.

---

### H4. Clear Title (Weight: High)
**Rule**: The title states exactly what task the guide accomplishes, starting with "How to..."

**Test**: Does the title complete the sentence "This guide shows you how to..."?

**Fail indicators**:
- Noun-phrase titles ("Database Configuration")
- Gerund titles ("Configuring the Database")  
- Vague titles ("Working with Data")

**Improvement**: Rewrite as "How to [verb] [object] [for purpose]": "How to configure database connections for high availability"

---

### H5. Handles Variations (Weight: High)
**Rule**: The guide acknowledges real-world variations and provides conditional paths.

**Test**: Does the guide address "What if my situation is slightly different?"

**Fail indicators**:
- Only works for one specific scenario
- No conditional branches
- Assumes identical environments

**Improvement**: Add conditional sections: "If you're using [alternative], do [X] instead." Use "If...then" structure for variations.

---

### H6. Logical Sequence (Weight: High)
**Rule**: Steps are ordered in a way that makes logical and practical sense.

**Test**: Does each step naturally follow from the previous? Does the order minimize context-switching?

**Fail indicators**:
- Steps that could be reordered arbitrarily
- Jumping between different tools/contexts
- Prerequisites discovered mid-guide

**Improvement**: Reorder to minimize cognitive load. Group related steps. State prerequisites before the steps that need them.

---

### H7. Omits Unnecessary Detail (Weight: High)
**Rule**: The guide includes only what's needed to accomplish the task—nothing more.

**Test**: For each piece of information, ask: "Does the user need this to complete the task?"

**Fail indicators**:
- Complete reference information embedded
- Historical context about the feature
- Exhaustive option lists

**Improvement**: Remove non-essential content. Link to Reference for complete options. Link to Explanation for background. Keep only what's needed to succeed.

---

### H8. Flexible Entry/Exit (Weight: Medium)
**Rule**: Users can start and stop at logical points; the guide doesn't require completing everything.

**Test**: Can a user who only needs steps 4-7 use just those steps?

**Fail indicators**:
- Steps tightly coupled when they needn't be
- Must complete all steps even for partial goals
- No logical stopping points

**Improvement**: Make steps as independent as possible. Add section headers for logical chunks. Note when steps can be skipped.

---

### H9. States the Goal Upfront (Weight: Medium)
**Rule**: The guide clearly states what will be accomplished before diving into steps.

**Test**: After reading the introduction, does the user know exactly what they'll achieve?

**Fail indicators**:
- Steps start immediately with no context
- Unclear what success looks like
- Goal only apparent after completion

**Improvement**: Open with: "This guide shows you how to [accomplish X]. By the end, you'll have [concrete outcome]."

---

## Reference Rules

Reference is **information-oriented**. It provides accurate, complete technical descriptions.

### R1. Accurate and Complete (Weight: Critical)
**Rule**: Every fact stated is correct, and no important facts are missing.

**Test**: Cross-check against the actual system. Is anything incorrect or omitted?

**Fail indicators**:
- Outdated information
- Parameters/options missing
- Behavior described incorrectly

**Improvement**: Audit against source of truth (code, API, system). Automate generation where possible. Establish review process for accuracy.

---

### R2. Description Only (Weight: Critical)
**Rule**: Reference describes what IS, not what to DO or WHY.

**Test**: Remove all sentences that tell the user to do something or explain reasoning. Is substantial content left?

**Fail indicators**:
- "You should use X when..."
- "To accomplish Y, do Z"
- "This exists because..."

**Improvement**: Convert instructions to descriptions: "X is used for..." Convert explanations to facts: "X accepts parameters A, B, C." Move procedures to How-to guides, reasoning to Explanation.

---

### R3. Mirrors System Structure (Weight: High)
**Rule**: The reference organization reflects the structure of what it documents.

**Test**: Can users navigate the reference using their knowledge of the system's structure?

**Fail indicators**:
- Organization by arbitrary categories
- Alphabetical when hierarchy exists
- Structure unrelated to system

**Improvement**: Reorganize to match system architecture. Modules contain classes; classes contain methods. APIs grouped by resource. Configuration grouped by component.

---

### R4. Consistent Format (Weight: High)
**Rule**: Every entry of the same type uses identical structure and formatting.

**Test**: Pick any two similar items (functions, parameters, endpoints). Are they formatted identically?

**Fail indicators**:
- Some functions have examples, others don't
- Inconsistent field ordering
- Variable levels of detail

**Improvement**: Create templates for each entry type. Apply templates uniformly. Automate formatting where possible.

---

### R5. Neutral Tone (Weight: High)
**Rule**: Language is objective and factual, without opinion or recommendation.

**Test**: Could any sentence be interpreted as advice or preference?

**Fail indicators**:
- "The recommended approach is..."
- "It's best to..."
- "You should prefer X over Y"

**Improvement**: Remove recommendations. State facts only: "X does [behavior]. Y does [different behavior]." Save recommendations for How-to guides or Explanation.

---

### R6. Authoritative (Weight: High)
**Rule**: Information is stated with certainty, no hedging or ambiguity.

**Test**: Is every statement definitive? Are there any "might," "should," "probably"?

**Fail indicators**:
- "This may cause..."
- "Usually this means..."
- Uncertain language

**Improvement**: Verify facts and state them definitively. If behavior is truly variable, describe the conditions: "When X, returns Y. When Z, returns W."

---

### R7. Includes Examples (Weight: Medium)
**Rule**: Abstract descriptions are illustrated with concrete examples.

**Test**: For each concept/parameter/function, is there at least one example of usage?

**Fail indicators**:
- Parameters described but never shown in use
- Syntax rules without examples
- Concepts without concrete instances

**Improvement**: Add minimal, focused examples. Examples should illustrate, not teach. One example per concept is often enough.

---

### R8. Easy to Scan (Weight: Medium)
**Rule**: Users can quickly find specific information without reading everything.

**Test**: Time how long it takes to find a specific fact. Is it under 30 seconds?

**Fail indicators**:
- Dense paragraphs
- No clear headings
- Important info buried in prose

**Improvement**: Use tables, lists, clear headings. Put key info in predictable locations. Bold or highlight important terms.

---

## Explanation Rules

Explanation is **understanding-oriented**. It provides context, background, and insight.

### E1. Illuminates Understanding (Weight: Critical)
**Rule**: After reading, the user understands something they didn't before—they have insight, not just information.

**Test**: Can the user now answer "why" questions they couldn't before?

**Fail indicators**:
- States facts without interpretation
- Describes "what" but not "why"
- No deeper insight provided

**Improvement**: For each topic, ask "Why is it this way? Why does it matter? How does it connect to other things?" Answer those questions.

---

### E2. No Actions Required (Weight: Critical)
**Rule**: The user doesn't need to DO anything—this is for understanding, not doing.

**Test**: Remove all imperative sentences ("Do X", "Run Y"). Does the document still make sense?

**Fail indicators**:
- Steps to follow
- Commands to run
- Procedures embedded in explanation

**Improvement**: Remove all procedural content. Move to How-to guides. Explanation should be readable away from the keyboard.

---

### E3. Provides Context (Weight: High)
**Rule**: The explanation places the topic in a broader context—historical, architectural, or conceptual.

**Test**: Does the reader understand where this fits in the bigger picture?

**Fail indicators**:
- Topic explained in isolation
- No connection to other concepts
- Missing "how we got here"

**Improvement**: Add context: How does this relate to X? Why was it designed this way? What problem does it solve? What came before?

---

### E4. Discusses Alternatives (Weight: High)
**Rule**: The explanation considers different approaches, trade-offs, and perspectives.

**Test**: Does the reader understand not just "what is" but "what could be" and "why this over that"?

**Fail indicators**:
- Only one approach presented
- No trade-offs discussed
- No "on the other hand..."

**Improvement**: Add: "An alternative approach is... The trade-off is... Some prefer X because... while others choose Y because..."

---

### E5. Bounded Scope (Weight: High)
**Rule**: The explanation has clear boundaries—it's about ONE topic, explored thoroughly.

**Test**: Can you state the topic in one phrase? Does everything relate to that topic?

**Fail indicators**:
- Scope creep into adjacent topics
- Trying to explain everything
- Unclear what the document is "about"

**Improvement**: Define scope explicitly. Use "About [X]" as a title test. Move tangential content to other Explanation documents.

---

### E6. Makes Connections (Weight: Medium)
**Rule**: The explanation links concepts together, showing relationships and dependencies.

**Test**: After reading, does the user see how pieces fit together?

**Fail indicators**:
- Concepts explained in isolation
- No "this relates to that because..."
- Missing the web of connections

**Improvement**: Explicitly state connections: "This is important because it affects X." "Unlike Y, this approach..." "This builds on the concept of Z."

---

### E7. Admits Perspective (Weight: Medium)
**Rule**: The explanation acknowledges that it represents a viewpoint, not absolute truth.

**Test**: Are opinions identified as opinions? Are multiple valid perspectives acknowledged?

**Fail indicators**:
- Opinions stated as facts
- Single perspective presented as only truth
- No acknowledgment of debate/alternatives

**Improvement**: Use framing: "One perspective is..." "Many practitioners prefer..." "The trade-off here is..." Acknowledge legitimate disagreement.

---

### E8. Readable Independently (Weight: Medium)
**Rule**: The explanation can be read and understood away from the product—in the bath, on a train.

**Test**: Does understanding require having the product open? Can it be read as prose?

**Fail indicators**:
- Requires following along in code
- Can't understand without doing
- Reads like a manual, not an essay

**Improvement**: Write for someone away from their computer. Use analogies. Explain concepts in terms of principles, not just product specifics.

---

## Scoring

For each document:

1. **Universal Rules**: 4 rules (all Critical)
2. **Type-Specific Rules**: 10 (Tutorial), 9 (How-to), 8 (Reference), 8 (Explanation)

**Scoring guide**:
- Critical rule failed: Document needs significant revision
- High weight rule failed: Document needs improvement in that area
- Medium weight rule failed: Nice-to-have improvement

**Minimum passing**: All Critical rules pass, majority of High rules pass.

---

## Quick Evaluation Checklist

### Any Document
- [ ] Serves exactly one purpose (U1)
- [ ] No type blurring (U2)
- [ ] Organized around user needs (U3)
- [ ] Links rather than inlines other types (U4)

### Tutorial
- [ ] User DOES things (T1)
- [ ] Every action has visible result (T2)
- [ ] Leads to accomplishment (T3)
- [ ] Works reliably (T4)
- [ ] Minimal explanation (T5)

### How-to Guide
- [ ] Solves real problem (H1)
- [ ] Assumes competence (H2)
- [ ] Action-focused (H3)
- [ ] "How to..." title (H4)
- [ ] Handles variations (H5)

### Reference
- [ ] Accurate and complete (R1)
- [ ] Description only (R2)
- [ ] Mirrors system structure (R3)
- [ ] Consistent format (R4)

### Explanation
- [ ] Illuminates understanding (E1)
- [ ] No actions required (E2)
- [ ] Provides context (E3)
- [ ] Discusses alternatives (E4)