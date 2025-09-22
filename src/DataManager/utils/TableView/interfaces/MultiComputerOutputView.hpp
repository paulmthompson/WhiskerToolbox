#ifndef MULTI_COMPUTER_OUTPUT_VIEW_HPP
#define MULTI_COMPUTER_OUTPUT_VIEW_HPP

#include "utils/TableView/interfaces/IColumnComputer.h"
#include "utils/TableView/interfaces/IMultiColumnComputer.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>


/**
 * @brief Adapter that exposes one output of an IMultiColumnComputer<T> as a normal IColumnComputer<T>.
 */
template <typename T>
class MultiComputerOutputView : public IColumnComputer<T> {
public:
    struct SharedBatchCache {
        mutable std::mutex mutex;
        mutable std::unordered_map<ExecutionPlan const *, std::pair<std::vector<std::vector<T>>, ColumnEntityIds>> cache;
    };

    explicit MultiComputerOutputView(std::shared_ptr<IMultiColumnComputer<T>> multiComputer,
                                     std::shared_ptr<SharedBatchCache> sharedCache,
                                     size_t outputIndex)
        : m_multiComputer(std::move(multiComputer)),
          m_sharedCache(std::move(sharedCache)),
          m_outputIndex(outputIndex) {}

    [[nodiscard]] std::pair<std::vector<T>, ColumnEntityIds> compute(ExecutionPlan const & plan) const override {
        // Fast path: return cached batch slice if available
        {
            std::lock_guard<std::mutex> lock(m_sharedCache->mutex);
            auto it = m_sharedCache->cache.find(&plan);
            if (it != m_sharedCache->cache.end()) {
                auto const & [batch, entity_ids] = it->second;
                if (m_outputIndex < batch.size()) {
                    return {batch[m_outputIndex], entity_ids};
                }
            }
        }

        // Compute batch and populate cache
        auto [batch, entity_ids] = m_multiComputer->computeBatch(plan);
        std::vector<T> result;
        if (m_outputIndex < batch.size()) {
            result = batch[m_outputIndex];
        }
        {
            std::lock_guard<std::mutex> lock(m_sharedCache->mutex);
            // Insert computed batch; move to avoid copies
            m_sharedCache->cache[&plan] = std::move(std::make_pair(std::move(batch), entity_ids));
        }
        return {result, entity_ids};
    }

    [[nodiscard]] auto getDependencies() const -> std::vector<std::string> override {
        return m_multiComputer->getDependencies();
    }

    [[nodiscard]] auto getSourceDependency() const -> std::string override {
        return m_multiComputer->getSourceDependency();
    }

    [[nodiscard]] auto getEntityIdStructure() const -> EntityIdStructure override {
        return m_multiComputer->getEntityIdStructure();
    }


    
private:
    std::shared_ptr<IMultiColumnComputer<T>> m_multiComputer;
    std::shared_ptr<SharedBatchCache> m_sharedCache;
    size_t m_outputIndex;
};

#endif // MULTI_COMPUTER_OUTPUT_VIEW_HPP


