# M2 Technical & Design Understanding

This file is an assessed technical-understanding artifact, not ordinary project documentation.
Answer all four questions using your own submitted implementation. Concise answers are acceptable when they are technically correct and specific.

Generic descriptions of C++ concepts or restatements of the assignment that do not identify and explain corresponding parts of your code will receive limited credit.

## 1. Polymorphism and dynamic dispatch - 1.5 points

Identify one place in your M2 implementation where runtime polymorphism occurs. Name the relevant base interface, derived implementation, and 'ProcessingCore' function involved. Trace the call from 'ProcessingCore' to the selected strategy implementation and explain why the derived implementation is invoked.

Then explain what would change if the relevant operation were not declared 'virtual'.

One example is 'ProcessingCore::search'. It calls 'impl_->retrieval->search(...)'. The pointer has type 'std::unique_ptr<RetrievalStrategy>', but the object inside can be a 'RetrievalEngine' or another derived class. In my test, I pass in 'MarkerRetrieval', and the search returns its score of 42.0. That shows the call reached the object I passed in. This works because 'RetrievalStrategy::search' is virtual. Without 'virtual', the 'override' in 'MarkerRetrieval' would not compile. If the base class had a regular nonvirtual function instead, a call through the base pointer would use that base function.

## 2. Ownership and lifetime - 1.5 points

Identify where one of the strategy objects is created, where ownership is transferred, and which object ultimately owns it. Explain how 'std::unique_ptr' represents that ownership relationship and when the strategy object is destroyed.

Also explain why 'ProcessingCore' is move-only and why the strategy base classes require virtual destructors.

In 'test_injected_pipeline_rebuild_and_moves', I create a 'MarkerRetrieval' with 'std::make_unique'. I pass it to the 'ProcessingCore' constructor. The constructor checks that none of the three pointers is null, then moves them into 'Impl'. From that point, the core owns them. 'std::unique_ptr' makes it clear that each strategy has one owner. The strategies are destroyed when the owning core is destroyed, or when move assignment replaces its old 'Impl'.

I also move the core in the test. The counters show that the injected strategies are each destroyed once. Copying 'ProcessingCore' is disabled because its owned strategies cannot just be copied. The base classes need virtual destructors so deleting a strategy through a base pointer also runs the derived class destructor.

## 3. Architecture, extensibility, and M1 compatibility - 1.5 points

Explain one specific architectural decision in your M2 implementation that makes the processing system extensible while preserving M1 behavior.

Identify the classes or interfaces involved and explain both:
- how the default configuration preserves M1 behavior; and
- how a different implementation can be substituted without changing the normal 'ProcessingCore' API.

Include one plausible design alternative and explain why the M2 design is preferable for this milestone. The alternative does not need to be something you actually implemented.

In M1, ProcessingCore directly kept a Chunker, RetrievalEngine, and ContextBuilder. In M2, Impl keeps them through ChunkingStrategy, RetrievalStrategy, and ContextStrategy. The default constructor still creates the M1 classes, so rebuild, search, and build_context work the same way as before. The other constructor lets me pass in my own strategies without changing those calls.

Another way would be an enum with switch statements in ProcessingCore. That would work, but I would need to edit the core each time I added an algorithm. The strategy interfaces let me add one as a separate class.

## 4. Testing and defect reasoning - 1.5 points

Select one meaningful test from your 'tests/student_tests.cpp'.

Explain:
- what M2 requirement the test validates;
- what specific implementation defect the test could detect; and
- why your test provides useful evidence beyond simply rerunning the supplied public tests.

If your test uses a custom strategy, explain how its observable behavior demonstrates that 'ProcessingCore' is actually using runtime substitution.

In test_injected_pipeline_rebuild_and_moves, I pass ProcessingCore three small test strategies. MarkerChunker adds '#marker' to the chunk ID. MarkerRetrieval checks that the chunk was indexed and returns 42.0 for a query the M1 search would not match. MarkerContext returns 'selected'. If ProcessingCore called one of the M1 classes instead, these checks would fail.

The same test tries a rebuild with duplicate document IDs. It should throw, and the old chunk should still be searchable. Then I move the core and check that each strategy is destroyed once. The public tests check basic injection; my test also checks the index, a failed rebuild, and ownership after a move.
