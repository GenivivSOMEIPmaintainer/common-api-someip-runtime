// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_OUTPUT_MESSAGE_STREAM_HPP_
#define COMMONAPI_SOMEIP_OUTPUT_MESSAGE_STREAM_HPP_

#include <cassert>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/OutputStream.hpp>

#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/Deployment.hpp>

namespace CommonAPI {
namespace SomeIP {

/**
 * Used to mark the position of a pointer within an array of bytes.
 */
typedef uint32_t position_t;

/**
 * @class OutputMessageStream
 *
 * Used to serialize and write data into a #Message. For all data types that may be written to a #Message, a "<<"-operator should be defined to handle the writing
 * (this operator is predefined for all basic data types and for vectors). The signature that has to be written to the #Message separately is assumed
 * to match the actual data that is inserted via the #OutputMessageStream.
 */
class OutputStream: public CommonAPI::OutputStream<OutputStream> {
public:

    /**
     * Creates a #OutputMessageStream which can be used to serialize and write data into the given #Message. Any data written is buffered within the stream.
     * Remember to call flush() when you are done with writing: Only then the data actually is written to the #Message.
     *
     * @param Message The #Message any data pushed into this stream should be written to.
     */
    COMMONAPI_EXPORT OutputStream(Message message);

    /**
     * Destructor; does not call the destructor of the referred #Message. Make sure to maintain a reference to the
     * #Message outside of the stream if you intend to make further use of the message, e.g. in order to send it,
     * now that you have written some payload into it.
     */
    COMMONAPI_EXPORT virtual ~OutputStream();

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const bool &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const int64_t &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const uint64_t &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const float &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const double &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const std::string &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual OutputStream &writeValue(const std::string &_value, const StringDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const ByteBuffer &_value, const ByteBufferDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &writeValue(const Version &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual OutputStream &_writeValue(const uint32_t &_value, const uint8_t &_width);
    COMMONAPI_EXPORT virtual OutputStream &_writeValueAt(const uint32_t &_value, const uint8_t &_width, const uint32_t &_position);

     template<typename Base_>
     COMMONAPI_EXPORT OutputStream &writeValue(const Enumeration<Base_> &_value, const EmptyDeployment *) {
         writeValue(static_cast<Base_>(_value), nullptr);
         return (*this);
     }

     template<class Deployment_, typename Base_>
     COMMONAPI_EXPORT OutputStream &writeValue(const Enumeration<Base_> &_value, const Deployment_ *_depl) {
         if (_depl != nullptr) {
             switch (_depl->width_) {
             case 1:
             {
                 uint8_t tmpValue1 = static_cast<uint8_t>(_value);
                 writeValue(tmpValue1, nullptr);
             }
             break;

             case 2:
             {
                 uint16_t tmpValue2 = static_cast<uint16_t>(_value);
                 writeValue(tmpValue2, nullptr);
             }
             break;

             default:
                 writeValue(static_cast<Base_>(_value), nullptr);
                 break;
             }
         } else {
             writeValue(static_cast<Base_>(_value), nullptr);
         }
         return (*this);
    }

    template<typename... Types_>
    COMMONAPI_EXPORT OutputStream &writeValue(const Struct<Types_...> &_value,
                             const EmptyDeployment *_depl) {
        // don't write length field as default length width is 0
        if(!hasError()) {
            // Write struct content
            const auto itsSize(std::tuple_size<std::tuple<Types_...>>::value);
            StructWriter<itsSize-1, OutputStream, Struct<Types_...>, EmptyDeployment>{}((*this), _value, _depl);
        }
        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT OutputStream &writeValue(const Struct<Types_...> &_value,
                             const Deployment_ *_depl) {
        uint8_t structLengthWidth = (_depl ? _depl->structLengthWidth_ : 0);

        if (structLengthWidth != 0) {
            pushPosition();
            // Length field placeholder
            _writeValue(0, structLengthWidth);
            pushPosition(); // Start of struct data
        }

        if(!hasError()) {
            // Write struct content
            const auto itsSize(std::tuple_size<std::tuple<Types_...>>::value);
            StructWriter<itsSize-1, OutputStream, Struct<Types_...>, Deployment_>{}((*this), _value, _depl);
        }

        // Write actual value of length field
        if (structLengthWidth != 0) {
            size_t length = getPosition() - popPosition();
            size_t position = popPosition();
            _writeValueAt(uint32_t(length), structLengthWidth, uint32_t(position));
        }

        return (*this);
    }

    template<class PolymorphicStruct_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::shared_ptr<PolymorphicStruct_> &_value,
                             const EmptyDeployment *_depl = nullptr) {
        if (_value) {
            _writeValue(_value->getSerial());
            if (!hasError()) {
                _value->template writeValue<OutputStream>((*this), _depl);
            }
        }
        return (*this);
    }

    template<class PolymorphicStruct_, typename Deployment_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::shared_ptr<PolymorphicStruct_> &_value,
                             const Deployment_ *_depl = nullptr) {
        if (_value) {
            _writeValue(_value->getSerial());
            if (!hasError()) {
                _value->template writeValue<OutputStream>((*this), _depl);
            }
        }
        return (*this);
    }
    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT OutputStream &writeValue(const Variant<Types_...> &_value,
                             const Deployment_ *_depl) {
        bool unionDefaultOrder = (_depl ? _depl->unionDefaultOrder_ : true);
        uint8_t unionLengthWidth = (_depl ? _depl->unionLengthWidth_ : 4);
        uint8_t unionTypeWidth = (_depl ? _depl->unionTypeWidth_ : 4);

        if (unionDefaultOrder) {
            pushPosition();
            _writeValue(0, unionLengthWidth);
            _writeValue(uint8_t(_value.getMaxValueType()) - _value.getValueType() + 1, unionTypeWidth);
            pushPosition();
        } else {
            _writeValue(uint8_t(_value.getMaxValueType()) - _value.getValueType() + 1, unionTypeWidth);
            pushPosition();
            _writeValue(0, unionLengthWidth);
            pushPosition();
        }

        if (!hasError()) {
            OutputStreamWriteVisitor<OutputStream> valueVisitor(*this);
            ApplyStreamVisitor<OutputStreamWriteVisitor<OutputStream>,
                 Variant<Types_...>, Deployment_, Types_...>::visit(valueVisitor, _value, _depl);
        }

        size_t length = getPosition() - popPosition();
        size_t position = popPosition();

        // Write actual value of length field
        if (unionLengthWidth != 0) {
            _writeValueAt(uint32_t(length), unionLengthWidth, uint32_t(position));
        } else {
            size_t paddingCount = _depl->unionMaxLength_ - length;
            if (paddingCount < 0) {
                errorOccurred_ = true;
            } else {
                for(size_t i = 0; i < paddingCount; i++) {
                    _writeValue('\0');
                }
            }
        }

        return (*this);
    }

    template<typename ElementType_, typename ElementDepl_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::vector<ElementType_> &_value,
                             const ArrayDeployment<ElementDepl_> *_depl) {
        uint8_t arrayLengthWidth = (_depl ? _depl->arrayLengthWidth_ : 4);
        uint32_t arrayMinLength = (_depl ? _depl->arrayMinLength_ : 0);
        uint32_t arrayMaxLength = (_depl ? _depl->arrayMaxLength_ : 0xFFFFFFFF);

        if (arrayLengthWidth != 0) {
            pushPosition();
            // Length field placeholder
            _writeValue(0, arrayLengthWidth);
            pushPosition(); // Start of vector data

            if (arrayMinLength != 0 && _value.size() < arrayMinLength) {
                errorOccurred_ = true;
            }
            if (arrayMaxLength != 0 && _value.size() > arrayMaxLength) {
                errorOccurred_ = true;
            }
        } else {
            if (arrayMaxLength != _value.size()) {
                errorOccurred_ = true;
            }
        }

        if (!hasError()) {
            // Write array/vector content
            for (auto i : _value) {
                writeValue(i, (_depl ? _depl->elementDepl_ : nullptr));
                if (hasError()) {
                    break;
                }
            }
        }

        // Write actual value of length field
        if (arrayLengthWidth != 0) {
            size_t length = getPosition() - popPosition();
            size_t position2Write = popPosition();
            _writeValueAt(uint32_t(length), arrayLengthWidth, uint32_t(position2Write));
        }

        return (*this);
    }

    template<typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value,
                             const EmptyDeployment *_depl) {
        pushPosition();
        _writeValue(static_cast<uint32_t>(0)); // Placeholder
        pushPosition(); // Start of map data

        for (auto v : _value) {
            writeValue(v.first, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                return (*this);
            }

            writeValue(v.second, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                return (*this);
            }
        }

        // Write number of written bytes to placeholder position
        uint32_t length = getPosition() - popPosition();
        _writeValueAt(length, popPosition());

        return (*this);
    }

    template<typename Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT OutputStream &writeValue(const std::unordered_map<KeyType_, ValueType_, HasherType_> &_value,
                             const Deployment_ *_depl) {
        pushPosition();
        _writeValue(static_cast<uint32_t>(0)); // Placeholder
        pushPosition(); // Start of map data

        for (auto v : _value) {
            writeValue(v.first, (_depl ? _depl->key_ : nullptr));
            if (hasError()) {
                return (*this);
            }

            writeValue(v.second, (_depl ? _depl->value_ : nullptr));
            if (hasError()) {
                return (*this);
            }
        }

        // Write number of written bytes to placeholder position
        uint32_t length = uint32_t(getPosition() - popPosition());
        _writeValueAt(length, popPosition());

        return (*this);
    }

    COMMONAPI_EXPORT virtual bool hasError() const;

    /**
     * Writes the data that was buffered within this #OutputMessageStream to the #Message that was given to the constructor. Each call to flush()
     * will completely override the data that currently is contained in the #Message. The data that is buffered in this #OutputMessageStream is
     * not deleted by calling flush().
     */
    COMMONAPI_EXPORT void flush();

    /**
     * Reserves the given number of bytes for writing, thereby negating the need to dynamically allocate memory while writing.
     * Use this method for optimization: If possible, reserve as many bytes as you need for your data before doing any writing.
     *
     * @param numOfBytes The number of bytes that should be reserved for writing.
     */
    COMMONAPI_EXPORT void reserveMemory(size_t numOfBytes);

    template<typename Type_>
    COMMONAPI_EXPORT OutputStream &_writeValue(const Type_ &_value) {
        union {
            Type_ typed;
            byte_t raw[sizeof(Type_)];
        } value;
        value.typed = _value;
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        byte_t *source = &value.raw[sizeof(Type_)-1];
        for (size_t i = 0; i < sizeof(Type_); ++i) {
            _writeRaw(*source--);
        }
    #else
        for (size_t i = 0; i < sizeof(Type_); ++i) {
            _writeRaw(value.raw[i]);
        }
    #endif
        return (*this);
    }

    template<typename Type_>
    COMMONAPI_EXPORT void _writeValueAt(const Type_ &_value, size_t _position) {
        assert(_position + sizeof(Type_) <= payload_.size());
        union {
            Type_ typed;
            byte_t raw[sizeof(Type_)];
        } value;
        value.typed = _value;
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        byte_t reordered[sizeof(Type_)];
        byte_t *source = &value.raw[sizeof(Type_)-1];
        byte_t *target = reordered;
        for (size_t i = 0; i < sizeof(Type_); ++i) {
            *target++ = *source--;
        }
        _writeRawAt(reordered, sizeof(Type_), _position);
    #else
        _writeRawAt(value.raw, sizeof(Type_), _position);
    #endif
    }

    /**
     * Fills the stream with 0-bytes to make the next value be aligned to the boundary given.
     * This means that as many 0-bytes are written to the buffer as are necessary
     * to make the next value start with the given alignment.
     *
     * @param _boundary The byte-boundary to which the next value should be aligned.
     */
    COMMONAPI_EXPORT void align(const size_t _boundary);

    /**
     * Takes sizeInByte characters, starting from the character which val points to, and stores them for later writing.
     * When calling flush(), all values that were written to this stream are copied into the payload of the #Message.
     *
     * The array of characters might be created from a pointer to a given value by using a reinterpret_cast. Example:
     * @code
     * ...
     * int32_t val = 15;
     * outputMessageStream.alignForBasicType(sizeof(int32_t));
     * const char* const reinterpreted = reinterpret_cast<const char*>(&val);
     * outputMessageStream.writeValue(reinterpreted, sizeof(int32_t));
     * ...
     * @endcode
     *
     * @param _data The array of chars that should serve as input
     * @param _size The number of bytes that should be written
     * @return true if writing was successful, false otherwise.
     *
     * @see OutputMessageStream()
     * @see flush()
     */
    COMMONAPI_EXPORT void _writeRaw(const byte_t &_data);
    COMMONAPI_EXPORT void _writeRaw(const byte_t *_data, const size_t _size);
    COMMONAPI_EXPORT void _writeRawAt(const byte_t *_data, const size_t _size, const size_t _position);

    COMMONAPI_EXPORT void _writeBom(const StringDeployment *_depl);

protected:
    std::vector<byte_t> payload_;

private:
    COMMONAPI_EXPORT size_t getPosition();
    COMMONAPI_EXPORT void pushPosition();
    COMMONAPI_EXPORT size_t popPosition();

    Message message_;
    bool errorOccurred_;

    std::stack<size_t> positions_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_OUTPUT_MESSAGE_STREAM_HPP_
