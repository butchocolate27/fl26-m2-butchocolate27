#include "aiws/chunking_strategy.hpp"
#include "aiws/context_strategy.hpp"
#include "aiws/processing_core.hpp"
#include "aiws/retrieval_strategy.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

struct Counts {
    int chunk_calls = 0;
    int retrieval_calls = 0;
    int context_calls = 0;
    int chunker_destructions = 0;
    int retrieval_destructions = 0;
    int context_destructions = 0;
};

class MarkerChunker final : public aiws::ChunkingStrategy {
public:
    explicit MarkerChunker(Counts& counts) : counts_(counts) {}
    ~MarkerChunker() override { ++counts_.chunker_destructions; }

    std::vector<aiws::Chunk> chunk(const aiws::Document& document,
                                   std::size_t order) const override {
        ++counts_.chunk_calls;
        return {{document.id() + "#marker", document.id(), order, 0,
                 "marker", 1, 0, document.text().size()}};
    }

private:
    Counts& counts_;
};

class MarkerRetrieval final : public aiws::RetrievalStrategy {
public:
    explicit MarkerRetrieval(Counts& counts) : counts_(counts) {}
    ~MarkerRetrieval() override { ++counts_.retrieval_destructions; }

    std::vector<aiws::SearchResult> search(const std::string&, int k,
                                            const std::vector<aiws::Chunk>& chunks,
                                            const aiws::CorpusIndex& index) const override {
        ++counts_.retrieval_calls;
        if (k <= 0 || chunks.empty()) return {};
        const auto& last = chunks.back();
        if (index.term_frequency("marker", last.id) != 1) {
            throw std::runtime_error("custom chunk was not indexed");
        }
        return {{last.id, last.document_id, last.sequence, last.text, 42.0, 1}};
    }

private:
    Counts& counts_;
};

class MarkerContext final : public aiws::ContextStrategy {
public:
    explicit MarkerContext(Counts& counts) : counts_(counts) {}
    ~MarkerContext() override { ++counts_.context_destructions; }

    std::vector<aiws::ContextItem> build(const std::vector<aiws::SearchResult>& ranked,
                                          std::size_t budget) const override {
        ++counts_.context_calls;
        if (ranked.empty() || budget == 0) return {};
        const auto& result = ranked.front();
        return {{result.chunk_id, result.document_id, result.chunk_sequence,
                 "selected", 1, result.score, false}};
    }

private:
    Counts& counts_;
};

void test_default_pipeline() {
    aiws::Workspace workspace;
    workspace.add_document({"doc1", "", "Search... SEARCH!! 42-times?"});
    aiws::ProcessingCore core;
    core.rebuild(workspace);

    check(core.chunk_count() == 1 && core.chunks()[0].text == "search search 42 times",
          "default chunker keeps M1 normalization");
    check(core.document_frequency("search") == 1 &&
              core.term_frequency("search", "doc1#0") == 2,
          "default corpus index counts terms");
    const auto ranked = core.search("SEARCH times?", 3);
    check(ranked.size() == 1 && ranked[0].chunk_id == "doc1#0" &&
              std::fabs(ranked[0].score - 2.962461898616) < 1e-12,
          "default retrieval keeps M1 score");
    const auto context = core.build_context("SEARCH times?", 3, 3);
    check(context.size() == 1 && context[0].text == "search search 42" &&
              context[0].truncated && context[0].token_count == 3,
          "default context keeps the M1 token budget");
}

void test_injected_pipeline_rebuild_and_moves() {
    Counts counts;
    {
        aiws::ProcessingCore core(std::make_unique<MarkerChunker>(counts),
                                  std::make_unique<MarkerRetrieval>(counts),
                                  std::make_unique<MarkerContext>(counts));
        aiws::Workspace valid;
        valid.add_document({"old", "", "ordinary text"});
        core.rebuild(valid);
        check(counts.chunk_calls == 1 && core.chunks()[0].id == "old#marker",
              "injected chunker is called by rebuild");
        const auto context = core.build_context("no default match", 1, 2);
        check(context.size() == 1 && context[0].text == "selected" &&
                  context[0].score == 42.0 && counts.retrieval_calls == 1 &&
                  counts.context_calls == 1,
              "injected retrieval and context strategies work together");

        aiws::Workspace duplicate;
        duplicate.add_document({"new", "", "first"});
        duplicate.add_document({"new", "", "second"});
        bool rejected = false;
        try {
            core.rebuild(duplicate);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && core.chunk_count() == 1 &&
                  core.chunks()[0].id == "old#marker" &&
                  core.search("anything", 1)[0].chunk_id == "old#marker",
              "failed rebuild retains the previous indexed corpus");

        aiws::ProcessingCore moved(std::move(core));
        check(moved.search("anything", 1)[0].chunk_id == "old#marker",
              "move construction transfers the strategies and corpus");
        aiws::ProcessingCore assigned;
        assigned = std::move(moved);
        check(assigned.build_context("anything", 1, 2)[0].text == "selected",
              "move assignment transfers the strategies and corpus");
    }
    check(counts.chunker_destructions == 1 && counts.retrieval_destructions == 1 &&
              counts.context_destructions == 1,
          "each injected strategy is destroyed exactly once after moves");
}

void test_null_configuration() {
    Counts counts;
    for (int missing = 0; missing < 3; ++missing) {
        std::unique_ptr<aiws::ChunkingStrategy> chunking;
        std::unique_ptr<aiws::RetrievalStrategy> retrieval;
        std::unique_ptr<aiws::ContextStrategy> context;
        if (missing != 0) chunking = std::make_unique<MarkerChunker>(counts);
        if (missing != 1) retrieval = std::make_unique<MarkerRetrieval>(counts);
        if (missing != 2) context = std::make_unique<MarkerContext>(counts);
        bool rejected = false;
        try {
            aiws::ProcessingCore invalid(std::move(chunking), std::move(retrieval),
                                         std::move(context));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "every null strategy position is rejected");
    }
}

}  // namespace

int main() {
    static_assert(std::is_abstract_v<aiws::ChunkingStrategy>);
    static_assert(std::is_abstract_v<aiws::RetrievalStrategy>);
    static_assert(std::is_abstract_v<aiws::ContextStrategy>);
    static_assert(!std::is_copy_constructible_v<aiws::ProcessingCore>);
    static_assert(!std::is_copy_assignable_v<aiws::ProcessingCore>);
    static_assert(std::is_move_constructible_v<aiws::ProcessingCore>);
    static_assert(std::is_move_assignable_v<aiws::ProcessingCore>);

    test_default_pipeline();
    test_injected_pipeline_rebuild_and_moves();
    test_null_configuration();
    if (failures == 0) std::cout << "M2 student tests passed\n";
    return failures == 0 ? 0 : 1;
}
