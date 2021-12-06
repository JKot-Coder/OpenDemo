#include "Tokenizer.hpp"

#include "common/LinearAllocator.hpp"

#include <iterator>

namespace RR
{
    namespace Rfx
    {
        namespace Compiler
        {
            namespace
            {
                inline bool isNewLineChar(U8Char ch)
                {
                    return (ch == '\n' || ch == '\r');
                }

                bool checkForEscapedNewline(const Tokenizer::const_iterator cursor, const Tokenizer::const_iterator end)
                {
                    ASSERT(*cursor == '\\')

                    U8Char next = 0;

                    // Peak next char if exist
                    if (std::distance(cursor, end) > 1)
                        next = *(cursor + 1);

                    return isNewLineChar(next);
                }

                void handleNewlineSequence(Tokenizer::const_iterator& cursor, const Tokenizer::const_iterator end)
                {
                    ASSERT(isNewLineChar(*cursor))

                    const auto first = *cursor;

                    if (++cursor == end)
                        return;

                    const auto second = *cursor;

                    // Handle all newline sequences
                    //  "\n"
                    //  "\r"
                    //  "\r\n"
                    //  "\n\r"
                    if (isNewLineChar(second) && first != second)
                        cursor++;
                }

                void handleEscapedNewline(Tokenizer::const_iterator& cursor, const Tokenizer::const_iterator end)
                {
                    ASSERT(checkForEscapedNewline(cursor, end))

                    cursor++;
                    handleNewlineSequence(cursor, end);
                }

                size_t scrubbingToken(const Tokenizer::const_iterator srcBegin, const Tokenizer::const_iterator srcEnd,
                                      U8Char* dstBegin, uint32_t& escapedLines)
                {
                    escapedLines = 0;
                    size_t lenght = 0;
                    auto cursor = srcBegin;
                    auto dst = dstBegin;

                    while (cursor != srcEnd)
                    {
                        if (*cursor == '\\')
                        {
                            if (checkForEscapedNewline(cursor, srcEnd))
                            {
                                escapedLines++;
                                handleEscapedNewline(cursor, srcEnd);
                                continue;
                            }
                        }
                       
                        lenght++;
                        *dst++ = *cursor++;
                    }

                    return lenght;
                }

            }

            Tokenizer::Tokenizer(const U8String& source)
                : cursor_(source.begin()),
                  end_(source.end()),
                  allocator_(new LinearAllocator(1024))
            {
            }

            Tokenizer::~Tokenizer()
            {
            }

            Token Tokenizer::GetNextToken()
            {
                if (isReachEOF())
                    return Token(Token::Type::Eof, nullptr, 0, line_);

                const auto tokenBegin = cursor_;

                bool scrubbingNeeded;
                const auto tokenType = scanToken(scrubbingNeeded);

                const auto tokenEnd = cursor_;

                const auto tokenLine = line_;

                if (tokenType == Token::Type::NewLine)
                    line_++;

                if (scrubbingNeeded)
                {
                    // "scrubbing" token value here to remove escaped newlines...
                    // Only perform this work if we encountered an escaped newline while lexing this token
                    // Allocate space that will always be more than enough for stripped contents
                    const size_t allocationSize = std::distance(tokenBegin, tokenEnd);
                    const auto beginDst = (U8Char*)allocator_->Allocate(allocationSize);

                    uint32_t escapedLines;
                    const auto scrubbledTokenLenght = scrubbingToken(tokenBegin, tokenEnd, beginDst, escapedLines);

                    // count escaped lines. Because of scrambling count of NewLineTokens != �ount of lines in file.
                    line_ += escapedLines;

                    return Token(tokenType, beginDst, scrubbledTokenLenght, tokenLine);
                }

                return Token(tokenType, &*tokenBegin, std::distance(tokenBegin, tokenEnd), tokenLine);
            }

            Token::Type Tokenizer::scanToken(bool& scrubbingNeeded)
            {
                ASSERT(!isReachEOF())

                scrubbingNeeded = false;
                auto ch = peek();

                switch (ch)
                {
                    case '\r':
                    case '\n':
                    {
                        handleNewlineSequence(cursor_, end_);
                        return Token::Type::NewLine;
                    }
                    default:
                    {
                        for (;;)
                        {
                            if (peek() == '\\')
                            {
                                if (checkForEscapedNewline(cursor_, end_))
                                {
                                    scrubbingNeeded = true;
                                    handleEscapedNewline(cursor_, end_);
                                }
                            }

                            if (isNewLineChar(peek()))
                                break;

                            if (!advance())
                                break;
                        }

                        return Token::Type::Lexeme;
                    }
                }
            }

        }
    }
}