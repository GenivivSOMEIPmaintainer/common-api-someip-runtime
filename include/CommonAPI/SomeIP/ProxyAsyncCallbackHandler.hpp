// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if !defined (COMMONAPI_INTERNAL_COMPILATION)
#error "Only <CommonAPI/CommonAPI.hpp> can be included directly, this file may disappear or change contents."
#endif

#ifndef COMMONAPI_SOMEIP_PROXY_ASYNC_CALLBACK_HANDLER_HPP_
#define COMMONAPI_SOMEIP_PROXY_ASYNC_CALLBACK_HANDLER_HPP_

#include <functional>
#include <future>
#include <memory>

#include <CommonAPI/SomeIP/Helper.hpp>
#include <CommonAPI/SomeIP/Message.hpp>
#include <CommonAPI/SomeIP/ProxyConnection.hpp>
#include <CommonAPI/SomeIP/SerializableArguments.hpp>

namespace CommonAPI {
namespace SomeIP {

template < typename ... ArgTypes_ >
class ProxyAsyncCallbackHandler: public ProxyConnection::MessageReplyAsyncHandler {
 public:
    typedef std::function< void(CallStatus, ArgTypes_...) > FunctionType;

    static std::unique_ptr< ProxyConnection::MessageReplyAsyncHandler > create(FunctionType &&callback, std::tuple< ArgTypes_... >&& _argTuple) {
        return std::unique_ptr< ProxyConnection::MessageReplyAsyncHandler >(
                new ProxyAsyncCallbackHandler(std::move(callback), std::move(_argTuple)));
    }

    ProxyAsyncCallbackHandler() = delete;
    ProxyAsyncCallbackHandler(FunctionType&& callback, std::tuple< ArgTypes_... >&& _argTuple):
        callback_(std::move(callback)),
        argTuple_(std::move(_argTuple)) {
    }

    virtual std::future< CallStatus > getFuture() {
        return promise_.get_future();
    }

    virtual void onMessageReply(const CallStatus &callStatus, const Message &message) {
        promise_.set_value(handleMessageReply(callStatus, message, typename make_sequence< sizeof...(ArgTypes_) >::type()));
    }

 private:
    template < int... ArgIndices_ >
    inline CallStatus handleMessageReply(const CallStatus _callStatus, const Message &message, index_sequence< ArgIndices_... >) const {
        CallStatus callStatus = _callStatus;
        std::tuple< ArgTypes_... > argTuple = argTuple_;

        (void) argTuple;

        if (callStatus == CallStatus::SUCCESS) {
            if (!message.isErrorType()) {
                InputStream inputStream(message);
                const bool success = SerializableArguments< ArgTypes_... >::deserialize(inputStream, std::get< ArgIndices_ >(argTuple)...);
                if (!success) {
                    callStatus = CallStatus::REMOTE_ERROR;
                }
            } else {
                callStatus = CallStatus::REMOTE_ERROR;
            }
        }

        callback_(callStatus, std::move(std::get< ArgIndices_ >(argTuple))...);
        return callStatus;
    }

    std::promise< CallStatus > promise_;
    const FunctionType callback_;
    std::tuple< ArgTypes_... > argTuple_;
};

} // namespace SomeIP
} // namespace CommonAPI

#endif // COMMONAPI_SOMEIP_PROXY_ASYNC_CALLBACK_HANDLER_HPP_
