#ifndef JINJA2CPP_SRC_TEMPLATE_IMPL_H
#define JINJA2CPP_SRC_TEMPLATE_IMPL_H

#include "internal_value.h"
#include "jinja2cpp/binding/rapid_json.h"
#include "jinja2cpp/template_env.h"
#include "jinja2cpp/value.h"
#include "renderer.h"
#include "template_parser.h"
#include "value_visitors.h"

#include <boost/optional.hpp>
#include <boost/predef/other/endian.h>
#include <expected>
#include <rapidjson/error/en.h>

#include <string>

namespace jinja2
{
namespace detail
{
template<size_t Sz>
struct RapidJsonEncodingType;

template<>
struct RapidJsonEncodingType<1>
{
    using type = rapidjson::UTF8<char>;
};

#ifdef BOOST_ENDIAN_BIG_BYTE
template<>
struct RapidJsonEncodingType<2>
{
    using type = rapidjson::UTF16BE<wchar_t>;
};

template<>
struct RapidJsonEncodingType<4>
{
    using type = rapidjson::UTF32BE<wchar_t>;
};
#else
template<>
struct RapidJsonEncodingType<2>
{
    using type = rapidjson::UTF16LE<wchar_t>;
};

template<>
struct RapidJsonEncodingType<4>
{
    using type = rapidjson::UTF32LE<wchar_t>;
};
#endif
} // namespace detail

extern void SetupGlobals(InternalValueMap& globalParams);

class ITemplateImpl
{
public:
    virtual ~ITemplateImpl() = default;
};


struct TemplateLoader
{
    static auto Load(const std::string& fileName, TemplateEnv* env)
    {
        return env->LoadTemplate(fileName);
    }
};

class GenericStreamWriter : public OutStream::StreamWriter
{
public:
    explicit GenericStreamWriter(std::string& os)
        : m_os(os)
    {}

    // StreamWriter interface
    void WriteBuffer(const void* ptr, size_t length) override
    {
        m_os.append(reinterpret_cast<const char*>(ptr), length);
    }
    void WriteValue(const InternalValue& val) override
    {
        Apply<visitors::ValueRenderer>(val, m_os);
    }

private:
    std::string& m_os;
};

class StringStreamWriter : public OutStream::StreamWriter
{
public:
    explicit StringStreamWriter(std::string* targetStr)
        : m_targetStr(targetStr)
    {}

    // StreamWriter interface
    void WriteBuffer(const void* ptr, size_t length) override
    {
        m_targetStr->append(reinterpret_cast<const char*>(ptr), length);
        // m_os.write(reinterpret_cast<const char*>(ptr), length);
    }
    void WriteValue(const InternalValue& val) override
    {
        Apply<visitors::ValueRenderer>(val, *m_targetStr);
    }

private:
    std::string* m_targetStr;
};

inline bool operator==(const TemplateEnv& lhs, const TemplateEnv& rhs)
{
    return lhs.IsEqual(rhs);
}
inline bool operator!=(const TemplateEnv& lhs, const TemplateEnv& rhs)
{
    return !(lhs == rhs);
}

inline bool operator==(const SourceLocation& lhs, const SourceLocation& rhs)
{
    if (lhs.fileName != rhs.fileName)
        return false;
    if (lhs.line != rhs.line)
        return false;
    if (lhs.col != rhs.col)
        return false;
    return true;
}
inline bool operator!=(const SourceLocation& lhs, const SourceLocation& rhs)
{
    return !(lhs == rhs);
}

inline bool operator==(const MetadataInfo& lhs, const MetadataInfo& rhs)
{
    if (lhs.metadata != rhs.metadata)
        return false;
    if (lhs.metadataType != rhs.metadataType)
        return false;
    if (lhs.location != rhs.location)
        return false;
    return true;
}

inline bool operator!=(const MetadataInfo& lhs, const MetadataInfo& rhs)
{
    return !(lhs == rhs);
}


class TemplateImpl : public ITemplateImpl
{
public:
    using ThisType = TemplateImpl;

    explicit TemplateImpl(TemplateEnv* env)
        : m_env(env)
    {
        if (env)
            m_settings = env->GetSettings();
    }

    auto GetRenderer() const {return m_renderer;}
    auto GetTemplateName() const {};

    boost::optional<ErrorInfoTpl> Load(std::string tpl, std::string tplName)
    {
        m_template = std::move(tpl);
        m_templateName = tplName.empty() ? std::string("noname.j2tpl") : std::move(tplName);
        TemplateParser parser(&m_template, m_settings, m_env, m_templateName);

        auto parseResult = parser.Parse();
        if (!parseResult)
            return parseResult.error()[0];

        m_renderer = *parseResult;
        m_metadataInfo = parser.GetMetadataInfo();
        return boost::optional<ErrorInfoTpl>();
    }

    boost::optional<ErrorInfoTpl> Render(std::string& os, const ValuesMap& params)
    {
        boost::optional<ErrorInfoTpl> normalResult;

        if (!m_renderer)
        {
            typename ErrorInfoTpl::Data errorData;
            errorData.code = ErrorCode::TemplateNotParsed;
            errorData.srcLoc.col = 1;
            errorData.srcLoc.line = 1;
            errorData.srcLoc.fileName = "<unknown file>";

            return ErrorInfoTpl(errorData);
        }

        try
        {
            InternalValueMap extParams;
            InternalValueMap intParams;

            auto convertFn = [&intParams](const auto& params) {
                for (auto& ip : params)
                {
                    auto valRef = &ip.second.data();
                    auto newParam = visit(visitors::InputValueConvertor(false, true), *valRef);
                    if (!newParam)
                        intParams[ip.first] = ValueRef(static_cast<const Value&>(*valRef));
                    else
                        intParams[ip.first] = newParam.get();
                }
            };

            if (m_env)
            {
                m_env->ApplyGlobals(convertFn);
                std::swap(extParams, intParams);
            }

            convertFn(params);
            SetupGlobals(extParams);

            RendererCallback callback(this);
            RenderContext context(intParams, extParams, &callback);
            InitRenderContext(context);
            OutStream outStream([writer = GenericStreamWriter(os)]() mutable -> OutStream::StreamWriter* {return &writer;});
            m_renderer->Render(outStream, context);
        }
        catch (const ErrorInfoTpl& error)
        {
            return error;
        }
        catch (const std::exception& ex)
        {
            typename ErrorInfoTpl::Data errorData;
            errorData.code = ErrorCode::UnexpectedException;
            errorData.srcLoc.col = 1;
            errorData.srcLoc.line = 1;
            errorData.srcLoc.fileName = m_templateName;
            errorData.extraParams.push_back(Value(std::string(ex.what())));

            return ErrorInfoTpl(errorData);
        }

        return normalResult;
    }

    InternalValueMap& InitRenderContext(RenderContext& context)
    {
        auto& curScope = context.GetCurrentScope();
        return curScope;
    }

    using TplLoadResultType = std::variant<EmptyValue,
            std::expected<std::shared_ptr<TemplateImpl>, ErrorInfo>>;

    using TplOrError = std::expected<std::shared_ptr<TemplateImpl>, ErrorInfoTpl>;

    TplLoadResultType LoadTemplate(const std::string& fileName)
    {
        if (!m_env)
            return TplLoadResultType(EmptyValue());

        auto tplWrapper = TemplateLoader::Load(fileName, m_env);
        if (!tplWrapper)
            return TplLoadResultType(TplOrError(std::unexpected{tplWrapper.error()}));

        return TplLoadResultType(TplOrError(std::static_pointer_cast<ThisType>(tplWrapper.value().m_impl)));
    }

    TplLoadResultType LoadTemplate(const InternalValue& fileName)
    {
        auto name = GetAsSameString(std::string(), fileName);
        if (!name)
        {
            typename ErrorInfoTpl::Data errorData;
            errorData.code = ErrorCode::InvalidTemplateName;
            errorData.srcLoc.col = 1;
            errorData.srcLoc.line = 1;
            errorData.srcLoc.fileName = m_templateName;
            errorData.extraParams.push_back(IntValue2Value(fileName));
            return TplOrError(std::unexpected(ErrorInfoTpl(errorData)));
        }

        return LoadTemplate(name.value());
    }

    std::expected<GenericMap, ErrorInfoTpl> GetMetadata() const
    {
        auto& metadataString = m_metadataInfo.metadata;
        if (metadataString.empty())
            return GenericMap();

        if (m_metadataInfo.metadataType == "json")
        {
            m_metadataJson = JsonDocumentType();
            rapidjson::ParseResult res = m_metadataJson.value().Parse(metadataString.data(), metadataString.size());
            if (!res)
            {
                typename ErrorInfoTpl::Data errorData;
                errorData.code = ErrorCode::MetadataParseError;
                errorData.srcLoc = m_metadataInfo.location;
                std::string jsonError = rapidjson::GetParseError_En(res.Code());
                errorData.extraParams.push_back(Value(std::move(jsonError)));
                return std::unexpected(ErrorInfoTpl(errorData));
            }
            m_metadata = std::move(std::get<GenericMap>(Reflect(m_metadataJson.value()).data()));
            return m_metadata.value();
        }
        return GenericMap();
    }

    std::expected<MetadataInfo, ErrorInfoTpl> GetMetadataRaw() const { return m_metadataInfo; }

    bool operator==(const TemplateImpl& other) const
    {
        if (m_env && other.m_env)
        {
            if (*m_env != *other.m_env)
                return false;
        }
        if (m_settings != other.m_settings)
            return false;
        if (m_template != other.m_template)
            return false;
        if (m_renderer && other.m_renderer && !m_renderer->IsEqual(*other.m_renderer))
            return false;
        if (m_metadata != other.m_metadata)
            return false;
        if (m_metadataJson != other.m_metadataJson)
            return false;
        if (m_metadataInfo != other.m_metadataInfo)
            return false;
        return true;
    }
private:
    void ThrowRuntimeError(ErrorCode code, ValuesList extraParams)
    {
        typename ErrorInfoTpl::Data errorData;
        errorData.code = code;
        errorData.srcLoc.col = 1;
        errorData.srcLoc.line = 1;
        errorData.srcLoc.fileName = m_templateName;
        errorData.extraParams = std::move(extraParams);

        throw ErrorInfoTpl(std::move(errorData));
    }

    class RendererCallback : public IRendererCallback
    {
    public:
        explicit RendererCallback(ThisType* host)
            : m_host(host)
        {}

        TargetString GetAsTargetString(const InternalValue& val) override
        {
            std::string os;
            Apply<visitors::ValueRenderer>(val, os);
            return TargetString(std::move(os));
        }

        OutStream GetStreamOnString(TargetString& str) override
        {
            using string_t = std::string;
            str = string_t();
            return OutStream([writer = StringStreamWriter(&std::get<string_t>(str))]() mutable -> OutStream::StreamWriter* { return &writer; });
        }

        std::variant<EmptyValue,
            std::expected<std::shared_ptr<TemplateImpl>, ErrorInfo>> LoadTemplate(const std::string& fileName) const override
        {
            return m_host->LoadTemplate(fileName);
        }

        std::variant<EmptyValue,
                std::expected<std::shared_ptr<TemplateImpl>, ErrorInfo>> LoadTemplate(const InternalValue& fileName) const override
        {
            return m_host->LoadTemplate(fileName);
        }

        void ThrowRuntimeError(ErrorCode code, ValuesList extraParams) override
        {
            m_host->ThrowRuntimeError(code, std::move(extraParams));
        }

        bool IsEqual(const IComparable& other) const override
        {
            auto* callback = dynamic_cast<const RendererCallback*>(&other);
            if (!callback)
                return false;
            if (m_host && callback->m_host)
                return *m_host == *(callback->m_host);
            if ((!m_host && (callback->m_host)) || (m_host && !(callback->m_host)))
                return false;
            return true;
        }
        bool operator==(const IComparable& other) const
        {
            auto* callback = dynamic_cast<const RendererCallback*>(&other);
            if (!callback)
                return false;
            if (m_host && callback->m_host)
                return *m_host == *(callback->m_host);
            if ((!m_host && (callback->m_host)) || (m_host && !(callback->m_host)))
                return false;
            return true;
        }

    private:
        ThisType* m_host;
    };
private:
    using JsonDocumentType = rapidjson::GenericDocument<typename detail::RapidJsonEncodingType<sizeof(char)>::type>;

    TemplateEnv* m_env{};
    Settings m_settings;
    std::string m_template;
    std::string m_templateName;
    RendererPtr m_renderer;
    mutable std::optional<GenericMap> m_metadata;
    mutable std::optional<JsonDocumentType> m_metadataJson;
    MetadataInfo m_metadataInfo;
};

} // namespace jinja2

#endif // JINJA2CPP_SRC_TEMPLATE_IMPL_H
