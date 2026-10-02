# M2 Technical & Design Understanding

This file is an assessed technical-understanding artifact, not ordinary project documentation.
Answer all four questions using your own submitted implementation. Concise answers are acceptable when they are technically correct and specific.

Generic descriptions of C++ concepts or restatements of the assignment that do not identify and explain corresponding parts of your code will receive limited credit.

## 1. Polymorphism and dynamic dispatch - 1.5 points

Identify one place in your M2 implementation where runtime polymorphism occurs. Name the relevant base interface, derived implementation, and `ProcessingCore` function involved. Trace the call from `ProcessingCore` to the selected strategy implementation and explain why the derived implementation is invoked.

Then explain what would change if the relevant operation were not declared `virtual`.

In `ProcessingCore::search`, `impl_->retrieval` has type `std::unique_ptr<RetrievalStrategy>`. Its `search` call goes through that base interface. The default object is a `RetrievalEngine`, while `test_injected_pipeline_rebuild_and_moves` supplies a `MarkerRetrieval`. Since `RetrievalStrategy::search` is virtual and the actual object is `MarkerRetrieval`, the call reaches `MarkerRetrieval::search` and returns its score of 42.0. The core does not need to know that derived class's name. If the base operation were not virtual, the derived method's `override` would fail to compile. With an ordinary nonvirtual base method instead, a call through the base pointer would use the base method rather than the selected derived method.

## 2. Ownership and lifetime - 1.5 points

Identify where one of the strategy objects is created, where ownership is transferred, and which object ultimately owns it. Explain how `std::unique_ptr` represents that ownership relationship and when the strategy object is destroyed.

Also explain why `ProcessingCore` is move-only and why the strategy base classes require virtual destructors.

In the test, `std::make_unique<MarkerRetrieval>(counts)` creates a strategy. The constructor call transfers its `unique_ptr` into `ProcessingCore`; the constructor checks all three arguments and moves them into `ProcessingCore::Impl`. That `Impl` owns the strategies until the core is destroyed or its ownership is moved to another core. In the move test, move construction and move assignment transfer the same `Impl`; the destruction counters show that each injected strategy is destroyed once at the end of the scope. `ProcessingCore` cannot be copied because that would imply copying its uniquely owned strategies, and there is no cloning contract. The virtual destructors make deletion through each `unique_ptr` to a base strategy safe, so the derived destructor runs.

## 3. Architecture, extensibility, and M1 compatibility - 1.5 points

Explain one specific architectural decision in your M2 implementation that makes the processing system extensible while preserving M1 behavior.

Identify the classes or interfaces involved and explain both:
- how the default configuration preserves M1 behavior; and
- how a different implementation can be substituted without changing the normal `ProcessingCore` API.

Include one plausible design alternative and explain why the M2 design is preferable for this milestone. The alternative does not need to be something you actually implemented.

I kept `Chunker`, `RetrievalEngine`, and `ContextBuilder` as the default concrete strategies and made the default `ProcessingCore` constructor pass those objects to the configurable constructor. This keeps the existing M1 chunking, scoring, and context algorithms. Both constructors then use the same `Impl` members, and `rebuild`, `search`, and `build_context` call through the three strategy interfaces. A caller can pass a different derived class to the configurable constructor without changing those normal core methods. An alternative would be an enum in `ProcessingCore` with a switch for each supported algorithm. That would require editing the core every time an algorithm is added, while the interface approach lets a caller supply a new implementation directly.

## 4. Testing and defect reasoning - 1.5 points

Select one meaningful test from your `tests/student_tests.cpp`.

Explain:
- what M2 requirement the test validates;
- what specific implementation defect the test could detect; and
- why your test provides useful evidence beyond simply rerunning the supplied public tests.

If your test uses a custom strategy, explain how its observable behavior demonstrates that `ProcessingCore` is actually using runtime substitution.

`test_injected_pipeline_rebuild_and_moves` checks the full custom path. `MarkerChunker` creates a chunk with a `#marker` ID and the word `marker`; `MarkerRetrieval` verifies that the new chunk was indexed and returns score 42.0 even for a query with no default match; `MarkerContext` returns the text `selected`. Those outputs could not come from the default M1 implementations, so the test detects a core that stores the injected strategies but still calls the concrete defaults. The same test also checks that a rebuild with duplicate document IDs throws while the old searchable corpus remains available. Its move and destruction checks can detect lost ownership or duplicate deletion. The public tests have a basic injection check, but they do not follow a failed rebuild and both move operations through this custom pipeline.
