#include "Archetype.hpp"

#include "ecs/EntityStorage.hpp"

namespace RR::Ecs
{
    ArchetypeEntityIndex Archetype::Insert(EntityStorage& entityStorage, EntityId entityId)
    {
        expand(1);
        auto index = ArchetypeEntityIndex(entityCount++);
        *(EntityId*)componentsData[0].GetData(index) = entityId;
        entityStorage.Mutate(entityId, id, index);
        return index;
    }

    ArchetypeEntityIndex Archetype::Mutate(EntityStorage& entityStorage, Archetype& from, ArchetypeEntityIndex fromIndex)
    {
        ASSERT(&from != this);

        expand(1);
        auto index = ArchetypeEntityIndex(entityCount++);

        for (size_t i = 0; i < componentsData.size(); i++)
        {
            const auto& componentInfo = componentsData[i].GetComponentInfo();
            std::byte* dst = componentsData[i].GetData(index);
            // TODO Could be faster find if we start from previous finded, to not iterate over same components id.
            // But this require componentsData to be sorted.
            const ComponentData* scrData = from.GetComponentData(componentInfo.id);
            if (!scrData)
                continue;

            std::byte* src = scrData->GetData(fromIndex);
            Move(componentInfo, dst, src);
        }

        entityStorage.Mutate(*(EntityId*)from.componentsData[0].GetData(fromIndex), id, index);
        from.Delete(entityStorage, fromIndex, false);

        return index;
    }

    void Archetype::Delete(EntityStorage& entityStorage, ArchetypeEntityIndex index, bool updateEntityRecord)
    {
        ASSERT(index.Value() < entityCount);
        const auto lastIndex = ArchetypeEntityIndex::FromValue(entityCount - 1);

        if (index != lastIndex)
            entityStorage.Move(*(EntityId*)componentsData[0].GetData(lastIndex), index);

        if (updateEntityRecord)
            entityStorage.Destroy(*(EntityId*)componentsData[0].GetData(index));

        for (auto& data : componentsData)
        {
            const auto& componentInfo = data.componentInfo;
            const auto removedPtr = data.GetData(index);

            if (componentInfo.destructor)
                componentInfo.destructor(removedPtr);

            if (index != lastIndex)
                Move(componentInfo, removedPtr, data.GetData(lastIndex));
        }

        entityCount--;
    }
}