#include "helpers.h"

#include <fmt/format.h>
#include <fmt/xchar.h>
#include <jinja2cpp/error_info.h>
#include <iterator>

namespace
{
template<typename FmtCtx>
struct ValueRenderer
{
    FmtCtx* ctx;

    explicit ValueRenderer(FmtCtx* c)
        : ctx(c)
    {
    }

    constexpr void operator()(bool val) const {
        fmt::format_to(
                ctx->out(),
                "{}",
                (val ? "True": "False"));
    }
    void operator()(const jinja2::EmptyValue&) const { fmt::format_to(ctx->out(), ""); }

    void operator()(const std::string& val) const
    {
        fmt::format_to(ctx->out(), "{}", std::string(val));
    }

    void operator()(const std::string_view& val) const
    {
        fmt::format_to(ctx->out(), "{}", std::string(val));
    }

    void operator()(const jinja2::ValuesList& vals) const
    {
        fmt::format_to(ctx->out(), "{{");
        bool isFirst = true;
        for (auto& val : vals)
        {
            if (isFirst)
                isFirst = false;
            else
                fmt::format_to(ctx->out(), ", ");
            std::visit(ValueRenderer<FmtCtx>(ctx), val.data());
        }
        fmt::format_to(ctx->out(), "}}");
    }

    void operator()(const jinja2::ValuesMap& vals) const
    {
        fmt::format_to(ctx->out(), "{{");
        bool isFirst = true;
        for (auto& val : vals)
        {
            if (isFirst)
                isFirst = false;
            else
                fmt::format_to(ctx->out(), ", ");

            fmt::format_to(ctx->out(), "{{\"{}\",", std::string(val.first));
            std::visit(ValueRenderer<FmtCtx>(ctx), val.second.data());
            fmt::format_to(ctx->out(), "}}");
        }
        fmt::format_to(ctx->out(), "}}");
    }

    template<typename T>
    void operator()(const jinja2::RecWrapper<T>& val) const
    {
        this->operator()(const_cast<const T&>(*val));
    }

    void operator()(const jinja2::GenericMap& /*val*/) const {}

    void operator()(const jinja2::GenericList& /*val*/) const {}

    void operator()(const jinja2::UserCallable& /*val*/) const {}

    template<typename T>
    void operator()(const T& val) const
    {
        fmt::format_to(ctx->out(), "{}", val);
    }
};
} // namespace

namespace fmt
{
template<>
struct formatter<jinja2::Value>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template<typename FormatContext>
    constexpr auto format(const jinja2::Value& val, FormatContext& ctx) const
    {
        std::visit(ValueRenderer<FormatContext>(&ctx), val.data());
        return fmt::format_to(ctx.out(), "");
    }
};
} // namespace fmt

namespace jinja2
{

void RenderErrorInfo(std::string& result, const ErrorInfoTpl& errInfo)
{
    using string_t = std::string;
    auto out = fmt::memory_buffer();

    auto& loc = errInfo.GetErrorLocation();

    fmt::format_to(std::back_inserter(out), "{}:{}:{}: error: ", loc.fileName, loc.line, loc.col);
    ErrorCode errCode = errInfo.GetCode();
    switch (errCode)
    {
    case ErrorCode::Unspecified:
            fmt::format_to(std::back_inserter(out), "Parse error");
            break;
    case ErrorCode::UnexpectedException:
    {
        auto& extraParams = errInfo.GetExtraParams();
        fmt::format_to(std::back_inserter(out), "Unexpected exception occurred during template processing. Exception: {}", extraParams[0]);
        break;
    }
    case ErrorCode::MetadataParseError:
    {
        auto& extraParams = errInfo.GetExtraParams();
        fmt::format_to(std::back_inserter(out), "Error occurred during template metadata parsing. Error: {}", extraParams[0]);
        break;
    }
    case ErrorCode::YetUnsupported:
        fmt::format_to(std::back_inserter(out), "This feature has not been supported yet");
        break;
    case ErrorCode::FileNotFound:
        fmt::format_to(std::back_inserter(out), "File not found");
        break;
    case ErrorCode::ExpectedStringLiteral:
        fmt::format_to(std::back_inserter(out), "String expected");
        break;
    case ErrorCode::ExpectedIdentifier:
        fmt::format_to(std::back_inserter(out), "Identifier expected");
        break;
    case ErrorCode::ExpectedSquareBracket:
        fmt::format_to(std::back_inserter(out), "']' expected");
        break;
    case ErrorCode::ExpectedRoundBracket:
        fmt::format_to(std::back_inserter(out), "')' expected");
        break;
    case ErrorCode::ExpectedCurlyBracket:
        fmt::format_to(std::back_inserter(out), "'}}' expected");
        break;
    case ErrorCode::ExpectedToken:
    {
        auto& extraParams = errInfo.GetExtraParams();
        fmt::format_to(std::back_inserter(out), "Unexpected token '{}'", extraParams[0]);
        if (extraParams.size() > 1)
        {
            fmt::format_to(std::back_inserter(out), ". Expected: ");
            for (std::size_t i = 1; i < extraParams.size(); ++ i)
            {
                if (i != 1)
                    fmt::format_to(std::back_inserter(out), ", ");
                fmt::format_to(std::back_inserter(out), "\'{}\'", extraParams[i]);
            }
        }
        break;
    }
    case ErrorCode::ExpectedExpression:
    {
        auto& extraParams = errInfo.GetExtraParams();
        fmt::format_to(std::back_inserter(out), "Expected expression, got: '{}'", extraParams[0]);
        break;
    }
    case ErrorCode::ExpectedEndOfStatement:
    {
        auto& extraParams = errInfo.GetExtraParams();
        fmt::format_to(std::back_inserter(out), "Expected end of statement, got: '{}'", extraParams[0]);
        break;
    }
    case ErrorCode::ExpectedRawEnd:
        fmt::format_to(std::back_inserter(out), "Expected end of raw block");
        break;
    case ErrorCode::ExpectedMetaEnd:
        fmt::format_to(std::back_inserter(out), "Expected end of meta block");
        break;
    case ErrorCode::UnexpectedToken:
    {
        auto& extraParams = errInfo.GetExtraParams();
        fmt::format_to(std::back_inserter(out), "Unexpected token: '{}'", extraParams[0]);
        break;
    }
    case ErrorCode::UnexpectedStatement:
    {
        auto& extraParams = errInfo.GetExtraParams();
        fmt::format_to(std::back_inserter(out), "Unexpected statement: '{}'", extraParams[0]);
        break;
    }
    case ErrorCode::UnexpectedCommentBegin:
        fmt::format_to(std::back_inserter(out), "Unexpected comment begin");
        break;
    case ErrorCode::UnexpectedCommentEnd:
        fmt::format_to(std::back_inserter(out), "Unexpected comment end");
        break;
    case ErrorCode::UnexpectedRawBegin:
        fmt::format_to(std::back_inserter(out), "Unexpected raw block begin");
        break;
    case ErrorCode::UnexpectedRawEnd:
        fmt::format_to(std::back_inserter(out), "Unexpected raw block end");
        break;
    case ErrorCode::UnexpectedMetaBegin:
        fmt::format_to(std::back_inserter(out), "Unexpected meta block begin");
        break;
    case ErrorCode::UnexpectedMetaEnd:
        fmt::format_to(std::back_inserter(out), "Unexpected meta block end");
        break;
    case ErrorCode::UnexpectedExprBegin:
        fmt::format_to(std::back_inserter(out), "Unexpected expression block begin");
        break;
    case ErrorCode::UnexpectedExprEnd:
        fmt::format_to(std::back_inserter(out), "Unexpected expression block end");
        break;
    case ErrorCode::UnexpectedStmtBegin:
        fmt::format_to(std::back_inserter(out), "Unexpected statement block begin");
        break;
    case ErrorCode::UnexpectedStmtEnd:
        fmt::format_to(std::back_inserter(out), "Unexpected statement block end");
        break;
    case ErrorCode::TemplateNotParsed:
        fmt::format_to(std::back_inserter(out), "Template not parsed");
        break;
    case ErrorCode::TemplateNotFound:
        fmt::format_to(std::back_inserter(out), "Template(s) not found: {}", errInfo.GetExtraParams()[0]);
        break;
    case ErrorCode::InvalidTemplateName:
        fmt::format_to(std::back_inserter(out), "Invalid template name: {}", errInfo.GetExtraParams()[0]);
        break;
    case ErrorCode::InvalidValueType:
        fmt::format_to(std::back_inserter(out), "Invalid value type");
        break;
    case ErrorCode::ExtensionDisabled:
        fmt::format_to(std::back_inserter(out), "Extension disabled");
        break;
    case ErrorCode::TemplateEnvAbsent:
        fmt::format_to(std::back_inserter(out), "Template environment doesn't set");
        break;
    }
    fmt::format_to(std::back_inserter(out), "\n{}", errInfo.GetLocationDescr());
    result = string_t{out.data(),out.size()};
}

std::string ErrorInfoTpl::ToString() const
{
    std::string result;
    RenderErrorInfo(result, *this);
    return result;
}

std::ostream& operator << (std::ostream& os, const ErrorInfo& res)
{
    os << res.ToString();
    return os;
}
} // namespace jinja2
