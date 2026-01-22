#include "ZoneManagerWidgetRegistration.hpp"
#include "ZoneManagerWidget.hpp"
#include "ZoneManager.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"

#include <rfl/json.hpp>

namespace {

/**
 * @brief Simple state data for ZoneManagerWidget
 * 
 * Stores persistent state like the last used config file path.
 */
struct ZoneManagerWidgetStateData {
    std::string instance_id;
    std::string last_config_path;
    bool auto_save_enabled = false;
    std::string auto_save_path;
};

/**
 * @brief EditorState implementation for ZoneManagerWidget
 */
class ZoneManagerWidgetState : public EditorState {
    Q_OBJECT

public:
    ZoneManagerWidgetState() = default;

    [[nodiscard]] QString getTypeName() const override { 
        return QStringLiteral("ZoneManagerWidget"); 
    }

    [[nodiscard]] std::string toJson() const override {
        // Store instance ID for restoration
        ZoneManagerWidgetStateData data_copy = _data;
        data_copy.instance_id = getInstanceId().toStdString();
        return rfl::json::write(data_copy);
    }

    bool fromJson(std::string const & json) override {
        auto result = rfl::json::read<ZoneManagerWidgetStateData>(json);
        if (result) {
            _data = *result;
            // Restore instance ID
            if (!_data.instance_id.empty()) {
                setInstanceId(QString::fromStdString(_data.instance_id));
            }
            emit stateChanged();
            return true;
        }
        return false;
    }

    void setLastConfigPath(QString const & path) {
        _data.last_config_path = path.toStdString();
        markDirty();
    }

    [[nodiscard]] QString lastConfigPath() const {
        return QString::fromStdString(_data.last_config_path);
    }

    void setAutoSaveEnabled(bool enabled) {
        _data.auto_save_enabled = enabled;
        markDirty();
    }

    [[nodiscard]] bool autoSaveEnabled() const {
        return _data.auto_save_enabled;
    }

    void setAutoSavePath(QString const & path) {
        _data.auto_save_path = path.toStdString();
        markDirty();
    }

    [[nodiscard]] QString autoSavePath() const {
        return QString::fromStdString(_data.auto_save_path);
    }

private:
    ZoneManagerWidgetStateData _data;
};

}  // anonymous namespace

#include "ZoneManagerWidgetRegistration.moc"

namespace ZoneManagerWidgetRegistration {

void registerType(EditorRegistry * registry, ZoneManager * zone_manager) {
    if (!registry || !zone_manager) {
        return;
    }

    EditorRegistry::EditorTypeInfo info;
    info.type_id = QStringLiteral("ZoneManagerWidget");
    info.display_name = QStringLiteral("Zone Layout Manager");
    info.menu_path = QStringLiteral("View/Layout");
    info.preferred_zone = Zone::Right;
    info.properties_zone = Zone::Right;
    info.allow_multiple = false;  // Single instance only

    // State factory
    info.create_state = []() -> std::shared_ptr<EditorState> {
        return std::make_shared<ZoneManagerWidgetState>();
    };

    // View factory - captures zone_manager
    info.create_view = [zone_manager](std::shared_ptr<EditorState> state) -> QWidget * {
        auto * widget = new ZoneManagerWidget(zone_manager);
        
        // Connect state persistence
        if (auto * zm_state = dynamic_cast<ZoneManagerWidgetState *>(state.get())) {
            QObject::connect(widget, &ZoneManagerWidget::configurationLoaded,
                             [zm_state](QString const & path) {
                                 zm_state->setLastConfigPath(path);
                             });
            QObject::connect(widget, &ZoneManagerWidget::configurationSaved,
                             [zm_state](QString const & path) {
                                 zm_state->setLastConfigPath(path);
                             });
        }
        
        return widget;
    };

    // No properties widget needed
    info.create_properties = nullptr;

    registry->registerType(std::move(info));
}

}  // namespace ZoneManagerWidgetRegistration
