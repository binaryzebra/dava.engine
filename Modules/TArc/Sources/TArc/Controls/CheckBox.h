#pragma once

#include "TArc/Controls/ControlProxy.h"
#include "TArc/Core/ContextAccessor.h"
#include "TArc/DataProcessing/DataWrappersProcessor.h"
#include "TArc/Utils/QtConnections.h"

#include <QCheckBox>

namespace DAVA
{
namespace TArc
{
class CheckBox final : public ControlProxy<QCheckBox>
{
public:
    struct FieldsDescriptor
    {
        Any valueFieldName;
    };

    CheckBox(const FieldsDescriptor& fields, DataWrappersProcessor* wrappersProcessor, Reflection model, QWidget* parent = nullptr);
    CheckBox(const FieldsDescriptor& fields, ContextAccessor* accessor, Reflection model, QWidget* parent = nullptr);

private:
    void OnDataChanged(const DataWrapper& wrapper, const Vector<Any>& fields) override;

    void SetupControl();
    void StateChanged(int newState);

private:
    FieldsDescriptor fieldsDescr;
    QtConnections connections;
};
} // namespace TArc
} // namespace DAVA
