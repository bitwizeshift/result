# Commit Standards

This project has some some requirements on how git commit messages are formed.

**General:**

* Commits should be small, granular, and easy to follow and revert. Ideally,
  the same SoC practices that would be applied to software development should
  be applied to commits; each commit identifies a separtion of concerns.

**Commit titles:**

* must be in imperitive, present-tense

* must be no longer than 50 characters

* must be prefixed by an [emoji key](#emoji-key) to indicate what the change
  is, followed by a space

  * If a commit may use more than one emoji to identify the change, it might
    indicate that the change is _not granular enough_, and you may be a
    candidate to be broken down into smaller commits for easier consumption

**Commit messages:**

* Must not exceed 72-character wide

  * Exceptions are made if a message contains a link or other figure that
    exceeds the 72 character rule by nature (e.g. code, compile message, etc)

* Must indicate the rationale for the change, what was changed, and why

  * In general, more details are always better to help identify the cause of
    changes in a repository

## Emoji Key

Emojis are used to prefix commit titles in order to simplify categorization
of git log messages.

Use the table below to identify which prefixes should be used for the
respective change:

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
| 🎨 | Cosmetic                                                               |
| 🩹 | Bug fix                                                                |
| 🧹 | Code Cleanup (includes moving types/files around)                      |
| ⏱ | Tuning / Performance                                                    |
| 🎯 | Testing (unit, benchmark, integration, etc)                            |
| 🔧 | Tooling                                                                |
| 🔐 | Security                                                               |
| ♿ | Accessibility                                                           |
| 🌐 | Localization / Internationalization                                    |
| 🚧 | WIP                                                                    |

**Note:** This list may be incomplete, and not cover all possible areas that
would be needed. Please feel free to start a discussion if new tags would be
more appropriate. Similarly, if there are more appropriate emojis to use as
tags, feel free to provide suggestions!
