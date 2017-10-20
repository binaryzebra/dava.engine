#pragma once

#include <TArc/DataProcessing/DataNode.h>
#include <TArc/WindowSubSystem/UI.h>

#include <Base/RefPtr.h>

class ControlNode;
class PackageNode;
class LibraryWidget;

namespace DAVA
{
namespace TArc
{
class QtAction;
}
}

class LibraryData : public DAVA::TArc::DataNode
{
public:
    LibraryData();
    ~LibraryData();

    const DAVA::Vector<DAVA::RefPtr<ControlNode>>& GetDefaultControls() const;

private:
    friend class LibraryModule;

    struct ActionInfo
    {
        DAVA::TArc::QtAction* action = nullptr;
        DAVA::TArc::ActionPlacementInfo placement;
    };
    using ActionsMap = DAVA::UnorderedMultiMap<ControlNode*, ActionInfo>;

    LibraryWidget* libraryWidget = nullptr;

    ActionsMap controlsActions;
    ActionsMap prototypesActions;

    DAVA::Vector<DAVA::RefPtr<ControlNode>> defaultControls;

    PackageNode* currentPackageNode = nullptr;

    DAVA_VIRTUAL_REFLECTION(LibraryData, DAVA::TArc::DataNode);
};