#pragma once

#include <string>

#include "common/VecMath.h"

namespace Common{
    class Stream;
}

namespace Render
{

    class Shader {
    public:

//        enum Attributes {
//            POSITION,
//            NORMAL,
//            TEX_COORD,
//            COLOR,
//            MAX_ATTRIBUTES
//        };
//
//        static const char* const AttributesNames[MAX_ATTRIBUTES] = { ""};


        enum UniformType {
            VIEW_PROJECTION_MATRIX,
            MODEL_MATRIX,
            UNIFORM_TYPE_MAX
        };

        static const char* const UniformsNames[UNIFORM_TYPE_MAX];

        virtual ~Shader() {};

        virtual bool LinkSource(Common::Stream *stream) = 0;
        virtual void Bind() const = 0;

        virtual void SetParam(UniformType uType, const Common::vec4 &value, int count = 1) const = 0;
        virtual void SetParam(UniformType uType, const Common::mat4 &value, int count = 1) const = 0;
        virtual void SetParam(UniformType uType, const Common::Basis &value, int count = 1) const = 0;
    };

}