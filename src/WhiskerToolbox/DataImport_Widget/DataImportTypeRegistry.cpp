#include "DataImportTypeRegistry.hpp"

DataImportTypeRegistry & DataImportTypeRegistry::instance() {
    static DataImportTypeRegistry registry;
    return registry;
}

void DataImportTypeRegistry::registerType(QString const & data_type, ImportWidgetFactory factory) {
    _factories[data_type] = std::move(factory);
}

bool DataImportTypeRegistry::hasType(QString const & data_type) const {
    return _factories.find(data_type) != _factories.end();
}

QWidget * DataImportTypeRegistry::createWidget(QString const & data_type,
                                                std::shared_ptr<DataManager> dm,
                                                QWidget * parent) const {
    auto it = _factories.find(data_type);
    if (it != _factories.end() && it->second.create_widget) {
        return it->second.create_widget(std::move(dm), parent);
    }
    return nullptr;
}

QStringList DataImportTypeRegistry::supportedTypes() const {
    QStringList types;
    types.reserve(static_cast<int>(_factories.size()));
    for (auto const & [type, factory] : _factories) {
        types.append(type);
    }
    return types;
}

QString DataImportTypeRegistry::displayName(QString const & data_type) const {
    auto it = _factories.find(data_type);
    if (it != _factories.end()) {
        return it->second.display_name;
    }
    return QString();
}
