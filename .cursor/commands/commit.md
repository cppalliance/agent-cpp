---
description: Stage all changes and commit with AI-generated message
---

# Git Commit with Generated Message

Stage all changes (new, modified, deleted files) and commit with a detailed message.

## Instructions

1. First, stage all changes:

```bash
git add -A
```

2. Show what will be committed:

```bash
git diff --cached --stat
```

3. Get the detailed diff for analysis:

```bash
git diff --cached
```

4. Based on the diff, generate a commit message following this format:
   - **Subject line**: Use conventional commits style (feat:, fix:, docs:, refactor:, test:, chore:, etc.) with a concise summary under 72 characters
   - **Body**: Add a blank line, then bullet points describing the specific changes

Example format:
```
feat: add compression support and improve error handling

- Add Brotli encode/decode functions in src/compress.cpp
- Fix null pointer check in path normalization
- Remove deprecated legacy API functions
- Update unit tests for new compression interface
```

5. Commit with the generated message:

```bash
git commit -m "<subject line>

<bullet points>"
```

6. Show the result:

```bash
git log -1 --oneline
```
