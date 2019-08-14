/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "mongo/logger/appender.h"
#include "mongo/logger/log_severity.h"
#include "mongo/logger/message_event.h"

namespace mongo {
namespace logger {

class MessageLogDomain {
    MessageLogDomain(const MessageLogDomain&) = delete;
    MessageLogDomain& operator=(const MessageLogDomain&) = delete;

public:

    class Appender { // can this be made from Appender<MessageEventEphemeral> ?
        public:
            // TODO The implementation needs to call logv2::doLog with the guts of the event
            Status append(const MessageEventEphemeral& event);
        private:
    };

    /**
     * Opaque handle returned by attachAppender(), which can be subsequently passed to
     * detachAppender() to detach an appender from an instance of MessageLogDomain.
     */
    class AppenderHandle {
        public:
            void reset();
    };
    class EventAppender{
    };

    MessageLogDomain();
    ~MessageLogDomain();

    /**
     * Receives an event for logging, calling append(event) on all attached appenders.
     *
     * If any appender fails, the behavior is determined by the abortOnFailure flag:
     * *If abortOnFailure is set, ::abort() is immediately called.
     * *If abortOnFailure is not set, the error is returned and no further appenders are called.
     */
    Status append(const MessageEventEphemeral& event);

    /**
     * Gets the state of the abortOnFailure flag.
     */
    bool getAbortOnFailure() const {
        return _abortOnFailure;
    }

    /**
     * Sets the state of the abortOnFailure flag.
     */
    void setAbortOnFailure(bool abortOnFailure) {
        _abortOnFailure = abortOnFailure;
    }

    //
    // Configuration methods.  Must be synchronized with each other and calls to "append" by the
    // caller.
    //

    /**
     * Attaches "appender" to this domain, taking ownership of it.  Returns a handle that may be
     * used later to detach this appender.
     */
    AppenderHandle attachAppender(std::unique_ptr<Appender>);

    //template <typename Ptr>
    //AppenderHandle attachAppender(Ptr appender) {
    //    return attachAppender(std::unique_ptr<EventAppender>(std::move(appender)));
    //}

    /**
     * Detaches the appender referenced by "handle" from this domain, releasing ownership of it.
     * Returns an unique_ptr to the handler to the caller, who is now responsible for its
     * deletion. Caller should consider "handle" is invalid after this call.
     */
    std::unique_ptr<EventAppender> detachAppender(AppenderHandle handle);

    /**
     * Destroy all attached appenders, invalidating all handles.
     */
    void clearAppenders();

private:
    std::vector<std::unique_ptr<EventAppender>> _appenders;
    bool _abortOnFailure;
};

}  // namespace logger
}  // namespace mongo
