with open('src/DataManager/DigitalTimeSeries/storage/OwningDigitalEventStorage.hpp', 'r') as f:
    content = f.read()

# I removed sizeImpl accidentally.
# Let's add it back inline.
if 'sizeImpl() const' not in content:
    content = content.replace('    void setEntityIds(std::vector<EntityId> ids);', '    void setEntityIds(std::vector<EntityId> ids);\n\n    // ========== CRTP Implementation ==========\n    [[nodiscard]] size_t sizeImpl() const { return _events.size(); }')

with open('src/DataManager/DigitalTimeSeries/storage/OwningDigitalEventStorage.hpp', 'w') as f:
    f.write(content)
