#include "TArc/Controls/PropertyPanel/ReflectedPropertyModel.h"
#include "TArc/Controls/PropertyPanel/ReflectedPropertyItem.h"
#include "TArc/Controls/PropertyPanel/Private/EmptyComponentValue.h"

#include "Debug/DVAssert.h"

namespace DAVA
{
namespace TArc
{
ReflectedPropertyModel::ReflectedPropertyModel(QPointer<QQmlEngine> engine_, QPointer<QtReflectionBridge> reflectionBridge_)
    : engine(engine_)
    , reflectionBridge(reflectionBridge_)
{
    rootItem.reset(new ReflectedPropertyItem(this, std::make_unique<EmptyComponentValue>()));

    RegisterExtension(ChildCreatorExtension::CreateDummy());
    RegisterExtension(MergeValuesExtension::CreateDummy());
    RegisterExtension(EditorComponentExtension::CreateDummy());

    using namespace std::placeholders;
    childCreator.nodeCreated.Connect(this, &ReflectedPropertyModel::ChildAdded);
    childCreator.nodeRemoved.Connect(this, &ReflectedPropertyModel::ChildRemoved);
}

ReflectedPropertyModel::~ReflectedPropertyModel()
{
    rootItem.reset();
}

//////////////////////////////////////
//       QAbstractItemModel         //
//////////////////////////////////////

int ReflectedPropertyModel::rowCount(const QModelIndex& parent) const
{
    return MapItem(parent)->GetChildCount();
}

int ReflectedPropertyModel::columnCount(const QModelIndex& parent) const
{
    return 2;
}

QVariant ReflectedPropertyModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        /// TODO UVR
        return QVariant::fromValue(MapItem(index));
    }

    return QVariant();
}

bool ReflectedPropertyModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    DVASSERT(false);
    return false;
}

QModelIndex ReflectedPropertyModel::index(int row, int column, const QModelIndex& parent) const
{
    ReflectedPropertyItem* item = MapItem(parent);
    return createIndex(row, column, item);
}

QModelIndex ReflectedPropertyModel::parent(const QModelIndex& index) const
{
    ReflectedPropertyItem* item = MapItem(index);
    return MapItem(item->parent);
}

//////////////////////////////////////
//       QAbstractItemModel         //
//////////////////////////////////////

void ReflectedPropertyModel::Update()
{
    //std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    Update(rootItem.get());
    //double duration = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - start).count();
    //NGT_TRACE_MSG("update duration : %f seconds\n", duration);
}

void ReflectedPropertyModel::Update(ReflectedPropertyItem* item)
{
    int32 propertyNodesCount = item->GetPropertyNodesCount();
    for (int32 i = 0; i < propertyNodesCount; ++i)
    {
        childCreator.UpdateSubTree(item->GetPropertyNode(i));
    }

    for (const std::unique_ptr<ReflectedPropertyItem>& child : item->children)
    {
        Update(child.get());
    }

    wrappersProcessor.Sync();
}

void ReflectedPropertyModel::SetObjects(const std::vector<Reflection>& objects)
{
    int childCount = static_cast<int>(rootItem->GetChildCount());
    if (childCount != 0)
    {
        nodeToItem.clear();
        beginRemoveRows(QModelIndex(), 0, childCount - 1);
        rootItem->RemoveChildren();
        endRemoveRows();
        rootItem->RemovePropertyNodes();
        childCreator.Clear();
    }

    for (const Reflection& obj : objects)
    {
        std::shared_ptr<PropertyNode> rootNode = childCreator.CreateRoot(obj);
        nodeToItem.emplace(rootNode, rootItem.get());
        rootItem->AddPropertyNode(rootNode);
    }

    Update();
}

void ReflectedPropertyModel::ChildAdded(std::shared_ptr<const PropertyNode> parent, std::shared_ptr<PropertyNode> node, size_t childPosition)
{
    auto iter = nodeToItem.find(parent);
    DVASSERT(iter != nodeToItem.end());

    ReflectedPropertyItem* parentItem = iter->second;

    ReflectedPropertyItem* childItem = GetExtensionChain<MergeValuesExtension>()->LookUpItem(node, parentItem->children);
    if (childItem != nullptr)
    {
        childItem->AddPropertyNode(node);
    }
    else
    {
        std::unique_ptr<BaseComponentValue> valueComponent = GetExtensionChain<EditorComponentExtension>()->GetEditor(node);
        valueComponent->Init(&wrappersProcessor, reflectionBridge.data());

        int32 childCount = parentItem->GetChildCount();
        QModelIndex parentIndex = MapItem(parentItem);

        beginInsertRows(parentIndex, childCount, childCount);
        ReflectedPropertyItem* childItem = parentItem->CreateChild(std::move(valueComponent));
        childItem->AddPropertyNode(node);
        endInsertRows();
    }

    auto newNode = nodeToItem.emplace(node, childItem);
    DVASSERT(newNode.second);
}

void ReflectedPropertyModel::ChildRemoved(std::shared_ptr<PropertyNode> node)
{
    auto iter = nodeToItem.find(node);
    assert(iter != nodeToItem.end());

    ReflectedPropertyItem* item = iter->second;
    item->RemovePropertyNode(node);
    nodeToItem.erase(node);

    if (item->GetPropertyNodesCount() == 0)
    {
        ReflectedPropertyItem* itemParent = item->parent;
        QModelIndex parentIndex = MapItem(itemParent);
        beginRemoveRows(parentIndex, item->position, item->position);
        itemParent->RemoveChild(item->position);
        endRemoveRows();
    }
}

ReflectedPropertyItem* ReflectedPropertyModel::MapItem(const QModelIndex& item) const
{
    if (item.isValid())
    {
        return reinterpret_cast<ReflectedPropertyItem*>(item.internalPointer());
    }

    return rootItem.get();
}

QModelIndex ReflectedPropertyModel::MapItem(ReflectedPropertyItem* item) const
{
    if (item == rootItem.get())
    {
        return QModelIndex();
    }

    return createIndex(item->position, 0, item->parent);
}

void ReflectedPropertyModel::RegisterExtension(const std::shared_ptr<ExtensionChain>& extension)
{
    const Type* extType = extension->GetType();
    if (extType == Type::Instance<ChildCreatorExtension>())
    {
        childCreator.RegisterExtension(PolymorphCast<ChildCreatorExtension>(extension));
        return;
    }

    auto iter = extensions.find(extType);
    if (iter == extensions.end())
    {
        extensions.emplace(extType, extension);
        return;
    }

    iter->second = ExtensionChain::AddExtension(iter->second, extension);
}

void ReflectedPropertyModel::UnregisterExtension(const std::shared_ptr<ExtensionChain>& extension)
{
    const Type* extType = extension->GetType();
    if (extType == Type::Instance<ChildCreatorExtension>())
    {
        childCreator.UnregisterExtension(PolymorphCast<ChildCreatorExtension>(extension));
        return;
    }

    auto iter = extensions.find(extType);
    if (iter == extensions.end())
    {
        /// you do something wrong
        DVASSERT(false);
        return;
    }

    iter->second = ExtensionChain::RemoveExtension(iter->second, extension);
}

QPointer<QQmlEngine> ReflectedPropertyModel::GetQmlEngine() const
{
    return engine;
}

} // namespace TArc
} // namespace DAVA
