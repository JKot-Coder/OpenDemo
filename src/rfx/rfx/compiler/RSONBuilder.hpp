#pragma once

#include "rfx/compiler/RSONValue.hpp"
#include "rfx/core/UnownedStringSlice.hpp"
#include <stack>

namespace RR
{
    namespace Common
    {
        enum class RResult;
    }

    namespace Rfx
    {
        using RResult = Common::RResult;

        struct CompileContext;
        struct SourceLocation;
        class DiagnosticSink;
        struct Token;

        class RSONBuilder
        {
        public:
            RResult StartObject();
            RSONValue EndObject();
            RResult StartArray();
            RSONValue EndArray();
            RResult Inheritance(const Token& initiatingToken, const RSONValue& parents);
            RResult AddValue(RSONValue value);
            RResult AddKeyValue(const Token& key, RSONValue value);
            /// Get the root value. Will be set after valid construction
            const RSONValue& GetRootValue() const { return root_; }

            RSONBuilder(const std::shared_ptr<CompileContext>& context);

        private:
            struct Context;

            DiagnosticSink& getSink() const;
            Context& currentContext() { return stack_.top(); }
            RSONValue& currentValue() { return currentContext().value; }

        private:
            using Parent = RSONValue::Container*;

            struct Context
            {
                Context(RSONValue value) : value(std::move(value)) {};
                std::vector<Parent> parents;
                RSONValue value;
            };

            std::stack<Context> stack_;
            RSONValue root_;
            std::shared_ptr<CompileContext> context_;
        };
    }
}