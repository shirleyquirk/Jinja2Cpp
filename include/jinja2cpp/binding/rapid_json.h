#ifndef JINJA2CPP_BINDING_RAPID_JSON_H
#define JINJA2CPP_BINDING_RAPID_JSON_H

#include <jinja2cpp/reflected_value.h>

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include <type_traits>

namespace jinja2
{
namespace detail
{

template<typename T>
class RapidJsonObjectAccessor : public IMapItemAccessor, public ReflectedDataHolder<T, false>
{
    static_assert(std::same_as<typename T::Ch,char>);
public:
    using ReflectedDataHolder<T, false>::ReflectedDataHolder;
    using ThisType = RapidJsonObjectAccessor<T>;
    ~RapidJsonObjectAccessor() override = default;

    size_t GetSize() const override
    {
        auto j = this->GetValue();
        return j ? j->MemberCount() : 0ULL;
    }

    bool HasValue(const std::string& name) const override
    {
        auto j = this->GetValue();
        return j ? j->HasMember(name.c_str()) : false;
    }

    Value GetValueByName(const std::string& name) const override
    {
        auto j = this->GetValue();
        if (!j || !j->HasMember(name.c_str()))
            return Value();

        return Reflect(&(*j)[name.c_str()]);
    }

    std::vector<std::string> GetKeys() const override
    {
        auto j = this->GetValue();
        if (!j)
            return {};

        std::vector<std::string> result;
        result.reserve(j->MemberCount());
        for (auto it = j->MemberBegin(); it != j->MemberEnd(); ++ it)
        {
            result.emplace_back(std::string_view(it->name.GetString()));
        }
        return result;
    }

    bool IsEqual(const IComparable& other) const override
    {
        auto* val = dynamic_cast<const ThisType*>(&other);
        if (!val)
            return false;
         auto enumerator = this->GetValue();
         auto otherEnum = val->GetValue();
         if (enumerator && otherEnum && enumerator != otherEnum)
             return false;
         if ((enumerator && !otherEnum) || (!enumerator && otherEnum))
             return false;
         return true;
    }
};

template<typename Enc>
struct RapidJsonArrayAccessor
    : IListItemAccessor
    , IIndexBasedAccessor
    , ReflectedDataHolder<rapidjson::GenericValue<Enc>, false>
{
    using ReflectedDataHolder<rapidjson::GenericValue<Enc>, false>::ReflectedDataHolder;
    using ThisType = RapidJsonArrayAccessor<Enc>;

    std::optional<size_t> GetSize() const override
    {
        auto j = this->GetValue();
        return j ? j->Size() : std::optional<size_t>();
    }

    const IIndexBasedAccessor* GetIndexer() const override
    {
        return this;
    }

    ListEnumeratorPtr CreateEnumerator() const override
    {
        using Enum = Enumerator<typename rapidjson::GenericValue<Enc>::ConstValueIterator>;
        auto j = this->GetValue();
        if (!j)
            return jinja2::ListEnumeratorPtr();

        return jinja2::ListEnumeratorPtr(new Enum(j->Begin(), j->End()));
    }

    Value GetItemByIndex(int64_t idx) const override
    {
        auto j = this->GetValue();
        if (!j)
            return Value();

        return Reflect((*j)[static_cast<rapidjson::SizeType>(idx)]);
    }

    bool IsEqual(const IComparable& other) const override
    {
        auto* val = dynamic_cast<const ThisType*>(&other);
        if (!val)
            return false;
         auto enumerator = this->GetValue();
         auto otherEnum = val->GetValue();
         if (enumerator && otherEnum && enumerator != otherEnum)
             return false;
         if ((enumerator && !otherEnum) || (!enumerator && otherEnum))
             return false;
         return true;
    }
};

template<typename Enc>
struct Reflector<rapidjson::GenericValue<Enc>>
{
    static Value CreateFromPtr(const rapidjson::GenericValue<Enc>* val)
    {
        Value result;
        switch (val->GetType())
        {
        case rapidjson::kNullType:
            break;
        case rapidjson::kFalseType:
            result = Value(false);
            break;
        case rapidjson::kTrueType:
            result = Value(true);
            break;
        case rapidjson::kObjectType:
            result = GenericMap([accessor = RapidJsonObjectAccessor<rapidjson::GenericValue<Enc>>(val)]() { return &accessor; });
            break;
        case rapidjson::kArrayType:
            result = GenericList([accessor = RapidJsonArrayAccessor<Enc>(val)]() { return &accessor; });
            break;
        case rapidjson::kStringType:
            result = std::basic_string<typename Enc::Ch>(val->GetString(), val->GetStringLength());
            break;
        case rapidjson::kNumberType:
            if (val->IsInt64() || val->IsUint64())
                result = val->GetInt64();
            else if (val->IsInt() || val->IsUint())
                result = val->GetInt();
            else
                result = val->GetDouble();
            break;
        }
        return result;
    }

};

template<typename Enc>
struct Reflector<rapidjson::GenericDocument<Enc>>
{
    static Value Create(const rapidjson::GenericDocument<Enc>& val)
    {
        return GenericMap([accessor = RapidJsonObjectAccessor<rapidjson::GenericDocument<Enc>>(&val)]() { return &accessor; });
    }

    static Value CreateFromPtr(const rapidjson::GenericDocument<Enc>* val)
    {
        return GenericMap([accessor = RapidJsonObjectAccessor<rapidjson::GenericDocument<Enc>>(val)]() { return &accessor; });
    }

};
} // namespace detail
} // namespace jinja2

#endif // JINJA2CPP_BINDING_RAPID_JSON_H
