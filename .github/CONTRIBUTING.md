# Contribution Guidelines

👋 Thank you for wanting to contribute to **Result**! \
This is a hobby project of mine that I'm excited to share with the C++
community, and I am thankful for any community involvement that can help this
project improve. 🙂

To make it as easy as possible for you to contribute, here are a few guidelines
which should help us avoid any unnecessary work or disappointment. And of
course, this document is subject to discussion, so please
[create an issue](https://github.com/bitwizeshift/result/issues/new/choose) or a
[discussion](https://github.com/bitwizeshift/result/discussions/new) if you
have any suggestions to improve it!

## Prerequisites

Before opening any pull requests, please make sure an Issue has been created and
*preferably* has been assigned to you. Note that this will require a
[Github account](https://github.com/signup/free).

## Contributing

### Did you find a bug?

If you think you've found a bug, please verify that an existing issue has not already been opened
before [opening a new issue](https://github.com/bitwizeshift/result/issues/new?template=bug_report.md).

Please clearly describe your issue using the respective template.

Make sure to describe how to **reproduce** the bug. If possible, attach a
complete example which demonstrates the error. Please also state what you
expected to happen instead of the error.

If you found a compilation error, please tell us which compiler (version and
operating system) you used and paste the (relevant part of) the error messages
to the ticket.

### Do you wish to suggest a new feature, or change an existing one?

If you wish to suggest a new feature or change an existing one, please open a
[discussion](https://github.com/bitwizeshift/result/discussions/new?category=ideas)
so we can talk the specifics of the suggestion before it comes to a pull-request.

If you propose a change or addition, try to give an example how the improved
code could look like or how to use it.

### Do you have a question about the library or project?

If you have any questions about the code or the project, please feel free to ask
your question in the respective
[Q&A Discussion](https://github.com/bitwizeshift/result/discussions/new?category=q-a)
section

### Are you opening a pull-request?

Please follow and fill in the pull-request template, and ensure that the PR
description clearly describes the problem and solution. Make sure to include a
reference to the relevant issue number. If this is a code change, ensure that
any new code attempts to follow existing conventions within the library, and
that all trailing whitespace is trimmed.

Please also ensure that your commits try to follow **Result**'s
[commit standards](../doc/commit-standards.md), and that you have a linear and
coherent history (e.g. no unnecessary `merge master` commits).

## Please Don't

* Please refrain from proposing changes that alter the minimum supported
  tooling or C++ version. \
  Many SDKs for embedded systems have limited or older support for compilers and
  toolchains, and as such this project strives to provide as much support to
  these systems as possible.

* Please do not open pull requests that address multiple issues. Open multiple
  pull requests, so that issues can be dealt with independently.

* Please avoid opening feature pull-requests without first starting a
  discussion. New features can take time to come to an agreement, and it's
  better to discuss these first to avoid any potential disappointment or
  disagreements that may arise during a review.
