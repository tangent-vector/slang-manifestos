Slang Evolution Process
=======================

This document describes the process that is used to *evolve* the Slang language, standard library, and compiler interfaces.
The process describes how features and other changes are to be proposed, discussed, implemented and iterated upon.
Our goal in defining and following this process is to continue to deliver new capabilities and services to users of the Slang language and toolset, while also providing enough stability that those users can easily build and maintain Slang codebases over years.

Scope
-----

Any change that affects the user-exposed *interface* of the Slang project should follow this process.
The interface includes:

* The features of the Slang language, including both syntax and semantics

* The public API of the Slang standard library

* The command-line interface and capabilities of the command-line `slangc` tool

* The public API of the Slang compiler shared library, exposed through `slang.h`

Additions, removals, and changes to the interfaces of those components are covered by this process.

Many kind of changes are not covered by the evolution process, most notably bug fixes and improvements to performance (of either generated code or the tools themselves);
those changes should follow the normal process for contributing to the codebase.

Other kinds of changes that are not covered by this process include:

* Experimental features, that are gated appropriately as described later in this document

* Documentation changes, including changes to documentation comments on public APIs and changes to the text of diagnostic messages.

* Tools used by contributors to the Slang project but not intended for users of Slang itself (e.g., `slang-test`)

* Related libraries or tools such as the Slang language server and the `slang-rhi` library, even if the code for those libraries/tools is hosted by the Slang project or is in the same repository as the compiler

* Changes to the non-error diagnostics that are emitted by the compiler, even when adding a new warning could cause compilation to fail for users who compile with warnings-as-errors.

Structure
---------

This document assumes that certain groups have been defined within the Slang project, which have not previously been spelled out.
This document doesn't seek to set up the definitions of these groups, their responsibilities, or the rules for how membership is determined.
Instead, the following are just informal:

* The **community** refers to everybody who uses Slang or follows the project.

* The **contributors** are the subset of the community who participate in the development of Slang, by contributing code, documentation, etc.

* The **committers** are the people who have been recognized as responsible community members and reliable contributors, and who have been given permission to approve contributions (such as pull requests for code changes) as part of normal development.

* The **core team** is a small group (likely 2-6 members) of committers who are responsible for defining and guiding the Slang language, and the overall strategy for the project.

Experimental Features
---------------------

This document assumes that the Slang project will have a well-defined process for language features and library APIs to be marked as *experimental*.
We do not intend to define that process here, nor the code changes required to implement that process.

Users of the Slang language and toolset should not be able to accidentally depend on, or even *use*, experimental features without an explicit opt-in.
The opt-in might be per-feature or a blanket opt-in for all experimental features.
Opt-in might be done via command-line flags, `#pragma`-like directives, etc.

Steps
-----

This section describes the steps that a change takes from inception to (possible) deployment as part of Slang.

### Pitch

A *pitch* for a change starts as a discussion thread on the main Slang GitHub project, in the appropriate "Evolution Pitch" category.
Pitches can be created by any community member, and there is no strict template that needs to be followed.

As part of triaging issues that get filed, committers may identify issues that represent enhancement requests or ideas and turn them into discussion threads for pitches.

Any community members may comment on the thread for a pitch.
More senior community members are expected to help guide the conversation in fruitful directions and head off unproductive conversation.

Committers may close pitch threads (or even issues that would otherwise be turned into pitch threads) if they are duplicates of, or overlap significantly with, existing evolution proposals that are further along in the process (even if those proposals have been rejected).

In order to move forward to the next step of the process, a pitch needs to have the following:

* One or more contributors who mutually agree to be responsible for the change, and to collaborate on both the proposal and implementation. These are the *owners* of the proposal

* At least one core team member who says that the change being pitched is something that the core team would consider.

* At least one committer (who could be the same as the core team member above) who approves of the technical plan that the owners of the proposal have laid out.

Contributors who want to see a pitch move forward should demonstrate that the most important design and implementation questions have been thought through.
It is expected that successful pitches will often include a strong first draft of a proposal document.

### Draft Proposal

If a pitch meets the requirements for moving forward, then the next step is for the owners of the proposal to create a first draft of a *proposal* document.

The steps to create a draft proposal are:

* Clone the `shader-slang/evolution` repository
* Copy the `0000-template.md` file to `0000-whatever-your-change-is.md`
* Fill out all of the sections of the template according to the prompts therein.
  * The "Authors" field should be set to the owners of the proposal.
  * The "Status" field should be set as "Draft."
* Create a pull request adding the new file

Pull requests for draft proposals should reference a pitch thread that met the requirements for moving forward.
A committer can approve the pull request to be merged, even if it is not complete, so long as it meets an acceptable quality bar.

As part of approving a draft proposal, the proposal document will be given the next available proposal number.
Subsequent steps may refer to the proposal by this number, or by a link to the document itself.

Once the draft proposal document is merged, the process moves to the next step.

### Implementation and Iteration

Once a draft proposal has been checked in, work on the implementation of the change may begin.
It is expected that implementation work will lead to subsequent revisions and improvements to the proposal document.

Implementation work should follow the ordinary policies for code contributions.
Notably, issues should be opened, tasks should be broken down if needed, work should be assigned to owners of the proposal.
Issues and pull requests for an in-progress proposal should reference the proposal number and/or document.

Committers are expected to work, under the guidance of the core team, to decide whether the implementation work for a given proposal should be undertaken in a branch, or on the `main` branch.
Proposals that are both large in scope and likely to be accepted (as determined by the core team) should be implemented on top of `main`, across multiple pull requests.
Smaller proposals, or those that the core team is less sure about should always be implemented in branches.

Whether or not a branch is used, all code changes that could affect the user-visible interface of the Slang system must be marked as experimental during this step.
If the technical implementation of "experimental mode" supports per-feature enables, then the proposal should be given a unique feature name.

The owners of a proposal should also contribute pull requests to update the proposal document as implementation experience helps to resolve the remaining open design questions.
There is no hard requirement at this step that the proposal document and implementation must stay in perfect sync.

Community members can download or build Slang packages that include partial implementations of an in-progress proposal and experiment with it (by enabling the appropriate features), and can review the in-progress proposal document.
Feedback can be given in discussion threads throughout the process, and the owners of the proposal are encourged to interact with community members, answer questions, and listen to feedback.

In order to move to the next step, the owners of the proposal must have:

* A complete draft of the proposal document, answering all major design questions about the change

* A complete implementation of the proposal, matching the design in the proposal document

* A committer who agrees that the proposal is potentially ready for approval, and who is willing to act as its shepherd for the following steps. The shepherd should not be one of the owners of the proposal.

### Community Review

Once a proposal is considered ready for approval, it can enter the community review step.
This step is primarily driven by the shepherd, rather than the owners of the proposal.

To drive the review process, the shepherd should do the following:

* Set the "Status" of the proposal to "Implemented - Under Review"

* Open a discussion thread titled "Community Review: SP-#### ...", following the template (that will be created) to prompt discussion. The first post of the thread must link to release packages that include support for the feature.

* Provide 7 days for community members to try out the feature, read the document, etc. and provide their feedback in the thread.

* At the end of that time, lock the thread

The shepherd and other senior community members should help guide the discussion in useful directions and head off unhelpful comments.
The owners of the proposal should pay attention to feedback in the review thread, but should be careful not to actively champion or defend their proposal; the proposal document and the implementation should speak for itself.

The owners of a proposal may decide to withdraw the proposal from review and take it back to the implementation and iteration step.
In this case, the shepherd will explain what is happening and lock the review thread.

Once the community review period is over, a proposal automatically moves to the next step.

### Core Team Decision

Once a proposal has made it through the community review process, the core team is expected to study the proposal document, implementation work, and community feedback and make a decision about the proposal.

A core team member will post a discussion thread titled "Accepted: SP-#### ..." for an accepted proposal, and similarly for the other possible decisions.
The first post in that thread should explain the rationale for the core team's decision, and any guidance or instructions from the core team to the shepherd or owners of the proposal.

The possible decisions are:

#### **Accepted**

The proposal will be accepted as-is, or with minor revisions as dictated by the core team.

The owners of the proposal are responsible for making any requested revisions to the proposal document and implementation.
The shepherd will handle review and approval of these pull requests, to confirm that they satisfy the requests from the core team.

The shepherd of the proposal is responsible for merging the pull request, if any, for the proposal implementation.
The shepherd will also change the status of the proposal document to "Accepted" and move the code changes for the feature out of experimental status.

When a release of Slang has been made that includes the proposal (as a non-experimental feature), the release manager for that release should change the status of the proposal document to "Released in ..." with the version number of that release.

#### **Rejected**

The proposal has be rejected entirely, and will be closed out.

The core team may specify that a proposed feature will never be accepted (and thus future proposals for the same basic thing should be shut down at the pitch step), or that a similar proposal might be accepted in the future under certain conditions.

The shepherd is responsible for marking the proposal document with a "Rejected" status.
If changes were made on top of `main`, the shepherd is responsible for coordinating with the contributors to remove the implementation of the proposal.

#### **Returned for Revisions**

The proposal is returned to the implementation and iteration step.

The core team will specify what revisions to the proposal document and/or implementation are expected to be made before it might be ready for review again.

The shepherd will set the status of the proposal document to "Returned for Revisions"

If the same proposal reached the community review step again in the future, the previous shepherd may choose to stay with the proposal or allow another committer to take over the role.

Additional Statuses
-------------------

While the preceding section covered the ordinary flow, there are additional statuses that a proposal may find itself in.
None of the following are expected to be commonly encountered cases.

### **Withdrawn** or **Abandoned**

At any time, the owners of a proposal may decide to *withdraw* it, whether because they no loinger believe the proposed change is a good one, or because they do not have the time or energy to continue owning it.

Similarly, it is possible that a proposal might go a long time without activity or updates from its owners, at which point the committers (under guidance from the core team) can mark it as abandoned.

### **Removed**

It is possible that at some point after a proposal is accepted, it is found that either the design or implementation was a mistake.
In such a case, the core team may make a decision (with or without community engagement) to remove a proposed feature.

The core team will communicate their decision as they would for an accepted/rejected/returned proposal, and will coordinate with the contributors to remove the code for the problematic feature and update the document.
