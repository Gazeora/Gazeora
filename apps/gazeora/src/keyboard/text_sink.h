#pragma once

namespace gazeora::app {

// Where the on-screen keyboard's typing goes.
class ITextSink {
public:
    virtual ~ITextSink() = default;

    virtual void Type(char32_t character) = 0;
    virtual void Erase() = 0;
    // Done: keep the text and stop typing.
    virtual void Finish() = 0;
};

}  // namespace gazeora::app
