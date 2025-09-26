#ifndef DATAMANAGERPARAMETER_WIDGET_HPP
#define DATAMANAGERPARAMETER_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

#include <QPointer>
#include <memory>

class DataManager; // fwd decl

/**
 * A TransformParameter widget base that opts into DataManager notifications.
 * - Provides setDataManager for derived widgets that need DataManager access.
 * - Safely observes DataManager using QPointer and queued UI updates.
 * - Avoids duplicate notifications after switching managers by checking the active manager.
 */
class DataManagerParameter_Widget : public TransformParameter_Widget {
    Q_OBJECT
public:
    using TransformParameter_Widget::TransformParameter_Widget;
    ~DataManagerParameter_Widget() override = default;

    // Opt-in: only widgets deriving from this class can accept a DataManager
    virtual void setDataManager(std::shared_ptr<DataManager> dm);

protected:
    std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    // Hooks for derived widgets
    virtual void onDataManagerChanged() {}
    virtual void onDataManagerDataChanged() {}

private:
    std::shared_ptr<DataManager> _data_manager;
    DataManager * _connected_dm{nullptr};
};

#endif // DATAMANAGERPARAMETER_WIDGET_HPP

