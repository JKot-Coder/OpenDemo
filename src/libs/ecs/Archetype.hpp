#pragma once

#include "ecs/ComponentTraits.hpp"
#include "ecs/ForwardDeclarations.hpp"
#include "ecs/Hash.hpp"
#include "ecs/Index.hpp"
#include "ecs/TypeTraits.hpp"
#include "ska/flat_hash_map.h"
#include <EASTL/fixed_vector.h>
#include <EASTL/sort.h>
#include <EASTL/vector_set.h>
#include <numeric>

namespace RR::Ecs
{
    using ArchetypeId = Index<struct ArchetypeIdTag, HashType>;
    using ArchetypeEntityIndex = Index<struct ArchetypeEntityIndexTag>;

    template <typename... Components>
    struct ArchetypeInfo
    {
    private:
        static constexpr HashType getArchetypeHash()
        {
            static_assert(eastl::is_same_v<HashType, uint64_t>, "Update hash combine function");

            uint64_t hash = 0xcbf29ce484222325;

            auto hash_64_fnv1a = [&](uint64_t value) {
                uint64_t prime = 0x100000001b3;
                hash = hash ^ value;
                hash *= prime;
            };

            (hash_64_fnv1a(GetTypeHash<Components>), ...);
            return hash;
        }

    public:
        static constexpr ArchetypeId Id = ArchetypeId::FromValue(getArchetypeHash());
    };

    template <typename Interator>
    static constexpr ArchetypeId GetArchetypeIdForComponents(Interator begin, Interator end)
    {
        static_assert(eastl::is_same_v<HashType, uint64_t>, "Update hash combine function");

        uint64_t hash = 0xcbf29ce484222325;

        auto hash_64_fnv1a = [&](uint64_t value) {
            uint64_t prime = 0x100000001b3;
            hash = hash ^ value;
            hash *= prime;
        };

        for (auto it = begin; it != end; ++it)
            hash_64_fnv1a((*it).Value());
        return ArchetypeId::FromValue(hash);
    }

    class Archetype
    {
    public:
        struct ComponentData
        {
            using Allocator = EASTLAllocatorType;

            // TODO maybe fixed chunch size in kb, not element counts.
            ComponentData(size_t chunkSizePower, ComponentInfo componentInfo, const Allocator& allocator)
                : componentInfo(componentInfo),
                  chunkSize(1 << chunkSizePower),
                  chunkSizePower(chunkSizePower),
                  chunkMask(chunkSize - 1),
                  containerAlignment(std::lcm(chunkSize, componentInfo.alignment)),
                  allocator(allocator)
            {
            }

            ~ComponentData()
            {
                for (std::byte* data : chunks)
                    allocator.deallocate(data, chunkSize * sizeOfElement());
            }

            void AllocateChunk()
            {
                capacity += chunkSize;
                chunks.push_back((std::byte*)allocator.allocate(chunkSize * sizeOfElement(), containerAlignment, 0));
            }

            std::byte* GetData(ArchetypeEntityIndex entityIndex) const
            {
                return GetData(entityIndex.Value() >> chunkSizePower, entityIndex.Value() & chunkMask);
            }

            std::byte* GetData(size_t chunk, size_t index) const
            {
                ASSERT(chunk < chunks.size());
                ASSERT(index < chunkSize);
                return chunks[chunk] + (index)*sizeOfElement();
            }

            const ComponentInfo& GetComponentInfo() const { return componentInfo; }

        private:
            friend class Archetype;
            size_t alignmentOfElement() const { return componentInfo.alignment; }
            size_t sizeOfElement() const { return componentInfo.size; }

        private:
            eastl::vector<std::byte*> chunks;
            ComponentInfo componentInfo;
            size_t chunkSize = 0;
            size_t chunkSizePower = 0;
            size_t chunkMask = 0;
            size_t containerAlignment = 0;
            size_t capacity = 0;
            Allocator allocator;
        };

    public:
        // Compomentn Info should be sorted
        template <typename Interator>
        Archetype(ArchetypeId id, size_t chunkSizePower, Interator compInfoBegin, Interator compInfoEnd)
            : id(id), chunkSize(1 << chunkSizePower)
        {
            for (Interator it = compInfoBegin; it != compInfoEnd; it++)
            {
                ComponentInfo& componentInfo = *it;
                componentsData.emplace_back(chunkSizePower, componentInfo, ComponentData::Allocator {});
                components.push_back_unsorted(componentInfo.id);
            }

            ASSERT(componentsData[0].componentInfo.id == GetComponentId<EntityId>);
        }

        ~Archetype()
        {
            for(auto& data : componentsData )
            {
                if (!data.componentInfo.destructor)
                    continue;

                for(size_t index = 0; index < entityCount; index++)
                    data.componentInfo.destructor(data.GetData(ArchetypeEntityIndex::FromValue(index)));
            }
        }

        // Component list should be sorted!
        template <class Iterator>
        bool HasComponents(Iterator compBegin, Iterator compEnd) const
        {
            return std::includes(components.begin(), components.end(), compBegin, compEnd);
        }

        template <typename ArgsTuple>
        void InitComponent(ArchetypeEntityIndex entityIndex, const ComponentInfo& componentInfo, ArgsTuple&& arg)
        {
            auto* componentData = GetComponentData(componentInfo.id);
            ASSERT(componentData != nullptr);

            componentData->GetData(entityIndex);
        }

        const ComponentData* GetComponentData(ComponentId componentId) const
        {
            auto it = components.find(componentId);
            return it != components.end() ? &componentsData[eastl::distance(components.begin(), it)] : nullptr;
        };

        ArchetypeEntityIndex Insert(EntityStorage& entityStorage, EntityId entityId);
        ArchetypeEntityIndex Mutate(EntityStorage& entityStorage, Archetype& from, ArchetypeEntityIndex fromIndex);
        void Delete(EntityStorage& entityStorage, ArchetypeEntityIndex index, bool updateEntityRecord = true);

        auto GetComponentsBegin() const { return components.begin(); }
        auto GetComponentsEnd() const { return components.end(); }
        size_t GetEntityCount() const { return entityCount; }
        size_t GetChunkCount() const { return chunkCount; }
        size_t GetChunkSize() const { return chunkSize; }

    private:
        void expand(size_t requiredEntityCount)
        {
            while (entityCount + requiredEntityCount > capacity)
            {
                capacity += chunkSize;
                chunkCount++;
                for (ComponentData& componentData : componentsData)
                {
                    componentData.AllocateChunk();
                    ASSERT(capacity == componentData.capacity);
                }
            }
        }

        ArchetypeId id;
        size_t entityCount = 0;
        size_t capacity = 0;
        size_t chunkSize = 0;
        size_t chunkCount = 0;
        eastl::vector<ComponentData> componentsData;
        FixedVectorSet<ComponentId, 64> components; // TODO magicNumber
    };
}