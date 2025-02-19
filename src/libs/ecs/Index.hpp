#pragma once

namespace RR::Ecs
{
    /**
     * @brief A strongly-typed index template class
     *
     * @tparam Tag Type tag used to differentiate different kinds of indices
     * @tparam IndexType Underlying type used to store the index value
     */
    template <typename Tag, typename IndexType = size_t>
    class Index
    {
    public:
        using ValueType = IndexType;
        static constexpr ValueType InvalidValue = static_cast<IndexType>(-1);
        static constexpr Index Invalid() { return Index{}; };

        constexpr Index() noexcept = default;
        explicit constexpr Index(ValueType value) noexcept : value_(value) { }

        static constexpr Index FromValue(ValueType value) noexcept { return Index(value); }

        constexpr IndexType Value() const noexcept { return value_; }
        constexpr bool IsValid() const noexcept { return value_ != InvalidValue; }

        constexpr bool operator==(const Index& other) const noexcept { return value_ == other.value_; }
        constexpr bool operator!=(const Index& other) const noexcept { return value_ != other.value_; }
        constexpr bool operator<(const Index& other) const noexcept { return value_ < other.value_; }
        constexpr bool operator<=(const Index& other) const noexcept { return value_ <= other.value_; }
        constexpr bool operator>(const Index& other) const noexcept { return value_ > other.value_; }
        constexpr bool operator>=(const Index& other) const noexcept { return value_ >= other.value_; }

    protected:
        IndexType value_ = InvalidValue;
    };
}

namespace eastl
{
    template <typename Tag, typename IndexType>
    struct hash<RR::Ecs::Index<Tag, IndexType>>
    {
        constexpr size_t operator()(const RR::Ecs::Index<Tag, IndexType>& index) const noexcept
        {
            return static_cast<size_t>(index.Value());
        }
    };
}