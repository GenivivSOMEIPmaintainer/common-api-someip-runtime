// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_INPUT_STREAM_HPP_
#define COMMONAPI_SOMEIP_INPUT_STREAM_HPP_

#include <CommonAPI/InputStream.hpp>
#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/Deployment.hpp>

#if defined(LINUX)
#include <endian.h>
#elif defined(FREEBSD)
#include <sys/endian.h>
#else
//#error "Undefined OS (only Linux/FreeBSD are currently supported)"
#endif

#include <stdint.h>
#include <cassert>
#include <cstring> // memset
#include <string>
#include <vector>
#include <stack>

#include <CommonAPI/Export.hpp>
#include <CommonAPI/SomeIP/Deployment.hpp>

namespace CommonAPI {
namespace SomeIP {

/**
 * @class InputMessageStream
 *
 * Used to deserialize and read data from a #Message. For all data types that can be read from a #Message, a ">>"-operator should be defined to handle the reading
 * (this operator is predefined for all basic data types and for vectors).
 */
class InputStream: public CommonAPI::InputStream<InputStream> {
public:
    COMMONAPI_EXPORT virtual bool hasError() const;

    COMMONAPI_EXPORT virtual InputStream &readValue(bool &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(int8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(int16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(int32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(int64_t &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(uint8_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(uint16_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(uint32_t &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(uint64_t &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(float &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(double &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(std::string &_value, const EmptyDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(std::string &_value, const StringDeployment *_depl);
    COMMONAPI_EXPORT virtual InputStream &readValue(ByteBuffer &_value, const ByteBufferDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(Version &_value, const EmptyDeployment *_depl);

    COMMONAPI_EXPORT virtual InputStream &readValue(uint32_t &_value, const uint8_t &_width, const bool &_permitZeroWidth);

    template<typename Base_>
    COMMONAPI_EXPORT InputStream &readValue(Enumeration<Base_> &_value, const EmptyDeployment *) {
        Base_ tmpValue;
        readValue(tmpValue, nullptr);
        _value = tmpValue;
        return (*this);
    }

    template<class Deployment_, typename Base_>
    COMMONAPI_EXPORT InputStream &readValue(Enumeration<Base_> &_value, const Deployment_ *_depl) {
        uint8_t width = (_depl ? _depl->width_ : 0);

        switch (width) {
        case 1:
            uint8_t tmpValue1;
            readValue(tmpValue1, nullptr);
            _value = static_cast<Base_>(tmpValue1);
            break;

        case 2:
            uint16_t tmpValue2;
            readValue(tmpValue2, nullptr);
            _value = static_cast<Base_>(tmpValue2);
            break;

        default:
            Base_ tmpValue;
            readValue(tmpValue, nullptr);
            _value = tmpValue;
            break;
        }

        return (*this);
    }

    template<typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Struct<Types_...> &_value,
                           const EmptyDeployment *_depl) {
        if (!hasError()) {
            const auto itsFieldSize(std::tuple_size<std::tuple<Types_...>>::value);
            StructReader<itsFieldSize-1, InputStream, Struct<Types_...>, EmptyDeployment>{}(
                (*this), _value, _depl);
        }
        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Struct<Types_...> &_value,
                           const Deployment_ *_depl) {
        uint32_t itsSize;
        uint8_t structLengthWidth = (_depl ? _depl->structLengthWidth_ : 0);

        // Read struct size
        readValue(itsSize, structLengthWidth, true);

        // Read struct fields, if reading size has been successful
        if (!hasError()) {
            size_t remainingBeforeRead = remaining_;

            const auto itsFieldSize(std::tuple_size<std::tuple<Types_...>>::value);
            StructReader<itsFieldSize-1, InputStream, Struct<Types_...>, Deployment_>{}(
                (*this), _value, _depl);

            if (structLengthWidth != 0) {
                size_t deserialized = remainingBeforeRead - remaining_;
                if (itsSize > deserialized) {
                    (void)_readRaw(itsSize - deserialized);
                }
            }
        }
        return (*this);
    }

    template<typename Deployment_, class Polymorphic_Struct>
    COMMONAPI_EXPORT InputStream &readValue(std::shared_ptr<Polymorphic_Struct> &_value,
                           const Deployment_ *_depl) {
        uint32_t serial;
        _readValue(serial);
        if (!hasError()) {
            _value = Polymorphic_Struct::create(serial);
            _value->template readValue<InputStream>(*this, _depl);
        }

        return (*this);
    }

    template<typename Deployment_, typename... Types_>
    COMMONAPI_EXPORT InputStream &readValue(Variant<Types_...> &_value,
                           const Deployment_ *_depl) {
        if(_value.hasValue()) {
            DeleteVisitor<_value.maxSize> visitor(_value.valueStorage_);
            ApplyVoidVisitor<DeleteVisitor<_value.maxSize>,
                Variant<Types_...>, Types_... >::visit(visitor, _value);
        }

        uint32_t itsSize;
        uint32_t itsType;

        bool unionDefaultOrder = (_depl ? _depl->unionDefaultOrder_ : true);
        uint8_t unionLengthWidth = (_depl ? _depl->unionLengthWidth_ : 4);
        uint8_t unionTypeWidth = (_depl ? _depl->unionTypeWidth_ : 4);

        if (unionDefaultOrder) {
            readValue(itsSize, unionLengthWidth, true);
            readValue(itsType, unionTypeWidth, false);
        } else {
            readValue(itsType, unionTypeWidth, false);
            readValue(itsSize, unionLengthWidth, true);
        }

        // CommonAPI variant supports only 255 different union types!
        _value.valueType_ = uint8_t((itsType > 255) ? 255 : uint8_t(_value.getMaxValueType()) - itsType + 1);

        if (!hasError()) {
            size_t remainingBeforeRead = remaining_;

            InputStreamReadVisitor<InputStream, Types_...> visitor((*this), _value);
            ApplyStreamVisitor<InputStreamReadVisitor<InputStream, Types_... >,
                Variant<Types_...>, Deployment_, Types_...>::visit(visitor, _value, _depl);

            size_t paddingCount = 0;
            if (unionLengthWidth != 0) {
                paddingCount = itsSize - (remainingBeforeRead - remaining_);
            } else {
                paddingCount = _depl->unionMaxLength_ - (remainingBeforeRead - remaining_);
            }
            if (paddingCount < 0) {
                errorOccurred_ = true;
            } else {
                (void)_readRaw(paddingCount);
            }
        }

        return (*this);
    }

    template<typename ElementType_, typename ElementDepl_>
    COMMONAPI_EXPORT InputStream &readValue(std::vector<ElementType_> &_value,
                           const ArrayDeployment<ElementDepl_> *_depl) {
        uint32_t itsSize;

        uint8_t arrayLengthWidth = (_depl ? _depl->arrayLengthWidth_ : 4);
        uint32_t arrayMinLength = (_depl ? _depl->arrayMinLength_ : 0);
        uint32_t arrayMaxLength = (_depl ? _depl->arrayMaxLength_ : 0xFFFFFFFF);

        // Read array size
        readValue(itsSize, arrayLengthWidth, true);

        // Reset target
        _value.clear();

        // Read elements, if reading size has been successful
        if (!hasError()) {
            while (itsSize > 0 || (arrayLengthWidth == 0 && _value.size() < arrayMaxLength)) {

                size_t remainingBeforeRead = remaining_;

                ElementType_ itsElement;
                readValue(itsElement, (_depl ? _depl->elementDepl_ : nullptr));
                if (hasError()) {
                    break;
                }

                _value.push_back(std::move(itsElement));

                if (arrayLengthWidth != 0) {
                    itsSize -= uint32_t(remainingBeforeRead - remaining_);
                }
            }

            if (arrayLengthWidth != 0) {
                if (itsSize != 0) {
                    errorOccurred_ = true;
                }
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


        }

        return (*this);
    }

    template<typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT InputStream &readValue(std::unordered_map<KeyType_,
                           ValueType_, HasherType_> &_value,
                           const EmptyDeployment *_depl) {

        typedef typename std::unordered_map<KeyType_, ValueType_, HasherType_>::value_type MapElement;

        uint32_t itsSize;
        _readValue(itsSize);

        while (itsSize > 0) {
            size_t remainingBeforeRead = remaining_;

            KeyType_ itsKey;
            readValue(itsKey, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                break;
            }

            ValueType_ itsValue;
            readValue(itsValue, static_cast<EmptyDeployment *>(nullptr));
            if (hasError()) {
                break;
            }

            _value.insert(MapElement(std::move(itsKey), std::move(itsValue)));

            itsSize -= uint32_t(remainingBeforeRead - remaining_);
        }

        if (itsSize != 0) {
            errorOccurred_ = true;
        }

        return (*this);
    }

    template<typename Deployment_, typename KeyType_, typename ValueType_, typename HasherType_>
    COMMONAPI_EXPORT InputStream &readValue(std::unordered_map<KeyType_,
                           ValueType_, HasherType_> &_value,
                           const Deployment_ *_depl) {

        typedef typename std::unordered_map<KeyType_, ValueType_, HasherType_>::value_type MapElement;

        uint32_t itsSize;
        _readValue(itsSize);

        while (itsSize > 0) {
            size_t remainingBeforeRead = remaining_;

            KeyType_ itsKey;
            readValue(itsKey, (_depl ? _depl->key_ : nullptr));
            if (hasError()) {
                break;
            }

            ValueType_ itsValue;
            readValue(itsValue, (_depl ? _depl->value_ : nullptr));
            if (hasError()) {
                break;
            }

            _value.insert(MapElement(std::move(itsKey), std::move(itsValue)));

            itsSize -= uint32_t(remainingBeforeRead - remaining_);
        }

        if (itsSize != 0) {
            errorOccurred_ = true;
        }

        return (*this);
    }

    /**
     * Creates a #InputMessageStream which can be used to deserialize and read data from the given #Message.
     * As no message-signature is checked, the user is responsible to ensure that the correct data types are read in the correct order.
     *
     * @param message the #Message from which data should be read.
     */
    COMMONAPI_EXPORT InputStream(const Message &_message);
    COMMONAPI_EXPORT InputStream(const InputStream &_stream) = delete;

    /**
     * Destructor; does not call the destructor of the referred #Message. Make sure to maintain a reference to the
     * #Message outside of the stream if you intend to make further use of the message.
     */
    COMMONAPI_EXPORT ~InputStream();

    /**
     * Aligns the stream to the given byte boundary, i.e. the stream skips as many bytes as are necessary to execute the next read
     * starting from the given boundary.
     *
     * @param _boundary the byte boundary to which the stream needs to be aligned.
     */
    COMMONAPI_EXPORT void align(const size_t _boundary);

    /**
     * Reads the given number of bytes and returns them as an array of characters.
     *
     * Actually, for performance reasons this command only returns a pointer to the current position in the stream,
     * and then increases the position of this pointer by the number of bytes indicated by the given parameter.
     * It is the user's responsibility to actually use only the number of bytes he indicated he would use.
     * It is assumed the user knows what kind of value is stored next in the #Message the data is streamed from.
     * Using a reinterpret_cast on the returned pointer should then restore the original value.
     *
     * Example use case:
     * @code
     * ...
     * inputMessageStream.alignForBasicType(sizeof(int32_t));
     * byte_t* const dataPtr = inputMessageStream.readRaw(sizeof(int32_t));
     * int32_t val = *(reinterpret_cast<int32_t*>(dataPtr));
     * ...
     * @endcode
     */
    COMMONAPI_EXPORT byte_t *_readRaw(const size_t _size);

    /**
     * Handles all reading of basic types from a given #DBusInputMessageStream.
     * Basic types in this context are: uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t, float, double.
     * Any types not listed here (especially all complex types, e.g. structs, unions etc.) need to provide a
     * specialized implementation of this operator.
     *
     * @tparam Type_ The type of the value that is to be read from the given stream.
     * @param _value The variable in which the retrieved value is to be stored
     * @return The given inputMessageStream to allow for successive reading
     */
    template<typename Type_>
    COMMONAPI_EXPORT bool _readValue(Type_ &_value) {
        bool isError(false);
        union {
            Type_ typed;
            char raw[sizeof(Type_)];
        } value;

        if (remaining_ < sizeof(Type_)) {
            std::memset(value.raw, 0, sizeof(Type_));
            isError = true;
        } else {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
            char *target = &value.raw[sizeof(Type_)-1];
            for (size_t i = 0; i < sizeof(Type_); ++i) {
                *target-- = *current_++;
            }
    #else
            std::memcpy(value.raw, current_, sizeof(Type_));
            current_ += sizeof(Type_);
    #endif
            remaining_ -= sizeof(Type_);
        }

         _value = value.typed;
         return (isError);
    }

private:
    byte_t* dataBegin_;
    byte_t* current_;
    size_t remaining_;
    Message message_;
    bool errorOccurred_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_INPUT_STREAM_HPP_
