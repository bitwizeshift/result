# Commit Standards

This project has some some basic standards for the `git` commit messages.


1. Commits should follow the 50/72 rule (max 50 columns for the title, 72 columns
  per description line<sup>[[1]](#exception)</sup>)
2. Commits should be small, granular, and independent.
3. Commit titles should be written in imperative, present-tense
4. Commit messages should, where clarification is beneficial, describe the
  "What", "Where", and "Why"

This is not required, but please consider prefixing your commit titles with an
[emoji key](#emoji-key), as listed below, to help categorize commit messages.

<sup id="exception">[1]: Exceptions are made for code snippets and URLs</sup>
## Emoji Key

Emojis are used to prefix commit titles in order to simplify categorization
of git log messages.

The table below identifies the common prefixes used in this project:

| Emoji | Reason                                                              |
|---|-------------------------------------------------------------------------|
| 🔖 | Version Tag                                                            |
| 📖 | Documentation / Textual Changes                                        |
| 📇 | Metadata (README, LICENSE, repo docs, etc)                             |
| 🚦 | Continuous Integration                                                 |
| ✨ | New Feature                                                             |
| ✏ | Rename                                                                  |
| 🔨 | Refactor                                                               |
| ⚠ | Deprecation                                                             |
| 🗑️ | Removal                                                               |
| 🩹 | Bug fix                                                                |
| 🧹 | Code Cleanup (includes moving types/files around)                      |
| ⏱ | Tuning / Performance                                                    |
| 🎯 | Testing (unit, benchmark, integration, etc)                            |
| 🔧 | Tooling                                                                |
| 🔐 | Security                                                               |
