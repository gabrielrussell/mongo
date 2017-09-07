/**
 * Copyright (C) 2017 MongoDB Inc.
 *
 * This program is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, the copyright holders give permission to link the
 * code of portions of this program with the OpenSSL library under certain
 * conditions as described in each individual source file and distribute
 * linked combinations including the program with the OpenSSL library. You
 * must comply with the GNU Affero General Public License in all respects
 * for all of the code used other than as permitted herein. If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so. If you do not
 * wish to do so, delete this exception statement from your version. If you
 * delete this exception statement from all source files in the program,
 * then also delete it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kControl

#include "mongo/platform/basic.h"

#include "mongo/db/logical_session_cache_impl.h"

#include "mongo/db/logical_session_id.h"
#include "mongo/db/logical_session_id_helpers.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/server_parameters.h"
#include "mongo/db/service_context.h"
#include "mongo/platform/atomic_word.h"
#include "mongo/util/duration.h"
#include "mongo/util/log.h"
#include "mongo/util/periodic_runner.h"

#include <iostream>

namespace mongo {

MONGO_EXPORT_STARTUP_SERVER_PARAMETER(logicalSessionRecordCacheSize,
                                      int,
                                      LogicalSessionCacheImpl::kLogicalSessionCacheDefaultCapacity);

MONGO_EXPORT_STARTUP_SERVER_PARAMETER(
    logicalSessionRefreshMinutes,
    int,
    LogicalSessionCacheImpl::kLogicalSessionDefaultRefresh.count());

constexpr int LogicalSessionCacheImpl::kLogicalSessionCacheDefaultCapacity;
constexpr Minutes LogicalSessionCacheImpl::kLogicalSessionDefaultRefresh;

LogicalSessionCacheImpl::LogicalSessionCacheImpl(std::unique_ptr<ServiceLiason> service,
                                                 std::unique_ptr<SessionsCollection> collection,
                                                 Options options)
    : _refreshInterval(options.refreshInterval),
      _sessionTimeout(options.sessionTimeout),
      _service(std::move(service)),
      _sessionsColl(std::move(collection)),
      _cache(options.capacity) {
    PeriodicRunner::PeriodicJob job{[this](Client* client) { _periodicRefresh(client); },
                                    duration_cast<Milliseconds>(_refreshInterval)};
    _service->scheduleJob(std::move(job));
}

LogicalSessionCacheImpl::~LogicalSessionCacheImpl() {
    try {
        _service->join();
    } catch (...) {
        // If we failed to join we might still be running a background thread,
        // log but swallow the error since there is no good way to recover.
        severe() << "Failed to join background service thread";
    }
}

Status LogicalSessionCacheImpl::promote(LogicalSessionId lsid) {
    stdx::unique_lock<stdx::mutex> lk(_cacheMutex);
    auto it = _cache.find(lsid);
    if (it == _cache.end()) {
        return {ErrorCodes::NoSuchSession, "no matching session record found in the cache"};
    }

    // Update the last use time before returning.
    it->second.setLastUse(now());
    return Status::OK();
}

Status LogicalSessionCacheImpl::startSession(OperationContext* opCtx, LogicalSessionRecord record) {
    // Add the new record to our local cache. We will insert it into the sessions collection
    // the next time _refresh is called. If there is already a record in the cache for this
    // session, we'll just write over it with our newer, more recent one.
    if (_addToCache(record)) {
        return Status{ErrorCodes::CacheEviction, "cache eviction"};
    }
    return Status::OK();
}

Status LogicalSessionCacheImpl::refreshSessions(OperationContext* opCtx,
                                                const RefreshSessionsCmdFromClient& cmd) {
    bool evictionOccured = false;
    // Update the timestamps of all these records in our cache.
    auto sessions = makeLogicalSessionIds(cmd.getRefreshSessions(), opCtx);
    for (auto& lsid : sessions) {
        if (!promote(lsid).isOK()) {
            // This is a new record, insert it.
            if (_addToCache(makeLogicalSessionRecord(opCtx, lsid, now()))) {
                evictionOccured = true;
            }
        }
    }
    if (evictionOccured) {
        return Status{ErrorCodes::CacheEviction, "cache eviction"};
    }
    return Status::OK();
}

Status LogicalSessionCacheImpl::refreshSessions(OperationContext* opCtx,
                                                const RefreshSessionsCmdFromClusterMember& cmd) {
    LogicalSessionRecordSet toRefresh{};

    // Update the timestamps of all these records in our cache.
    auto records = cmd.getRefreshSessionsInternal();
    for (auto& record : records) {
        if (!promote(record.getId()).isOK()) {
            // This is a new record, insert it.
            _addToCache(record);
        }
        toRefresh.insert(record);
    }

    // Write to the sessions collection now.
    return _sessionsColl->refreshSessions(opCtx, toRefresh, now());
}

void LogicalSessionCacheImpl::vivify(OperationContext* opCtx, const LogicalSessionId& lsid) {
    if (!promote(lsid).isOK()) {
        startSession(opCtx, makeLogicalSessionRecord(opCtx, lsid, now())).ignore();
    }
}

Status LogicalSessionCacheImpl::refreshNow(Client* client) {
    return _refresh(client);
}

Date_t LogicalSessionCacheImpl::now() {
    return _service->now();
}

size_t LogicalSessionCacheImpl::size() {
    stdx::lock_guard<stdx::mutex> lock(_cacheMutex);
    return _cache.size();
}

void LogicalSessionCacheImpl::_periodicRefresh(Client* client) {
    auto res = _refresh(client);
    if (!res.isOK()) {
        log() << "Failed to refresh session cache: " << res;
    }

    return;
}

Status LogicalSessionCacheImpl::_refresh(Client* client) {
    LogicalSessionRecordSet activeSessions;
    LogicalSessionIdSet staleSessions;
    LogicalSessionIdSet deadSessions;

    auto time = now();

    // get or make an opCtx

    boost::optional<ServiceContext::UniqueOperationContext> uniqueCtx;
    auto* const opCtx = [&client, &uniqueCtx] {
        if (client->getOperationContext()) {
            return client->getOperationContext();
        }

        uniqueCtx.emplace(client->makeOperationContext());
        return uniqueCtx->get();
    }();

    // Promote our cached entries for all active service sessions to be recently used

    auto serviceSessions = _service->getActiveSessions();
    for (auto lsid : serviceSessions) {
        vivify(opCtx, lsid);
    }

    // grab the sessions that have been requested to end
    {
        using std::swap;
        stdx::unique_lock<stdx::mutex> lk(_cacheMutex);
        swap(deadSessions, _endingSessions);
    }

    // remove the requested-to-end sessions from the sessions collection

    if (!_sessionsColl->removeRecords(opCtx, deadSessions).isOK()) {
        // if we've failed to remove records, then replace _endingSessions
        // and add any sessions to end that may have queued while the lock was not held
        using std::swap;
        stdx::unique_lock<stdx::mutex> lk(_cacheMutex);
        swap(deadSessions, _endingSessions);
        for (auto& lsid : deadSessions) {
            _endingSessions.emplace(lsid);
        }
    }

    // find both stale and active records

    std::vector<decltype(_cache)::ListEntry> cacheCopy;
    {
        stdx::unique_lock<stdx::mutex> lk(_cacheMutex);
        cacheCopy.assign(_cache.begin(), _cache.end());
    }
    for (const auto& it : cacheCopy) {
        // std::cerr << "checking cache entry " << it.first.getId() << " ... ";
        const auto& record = it.second;
        if (_isStale(record, time)) {
            // std::cerr << "stale\n";
            staleSessions.insert(record.getId());
        } else if (deadSessions.count(record.getId()) > 0) {
            // std::cerr << "dead\n";
            // noop
        } else if (_isRecientlyActive(record)) {
            // std::cerr << "active\n";
            activeSessions.insert(record);
        } else {
            // std::cerr << "nothing\n";
        }
    }

    // find which locally stale sessions are also externally expired

    auto statusAndRemovedSessions = _sessionsColl->findRemovedSessions(opCtx, staleSessions);
    if (statusAndRemovedSessions.isOK()) {
        auto removedSessions = statusAndRemovedSessions.getValue();
        deadSessions.insert(removedSessions.begin(), removedSessions.end());
    } else {
    }

    // erase the truelyDeadSessions sessions from the cache
    // and prepare to kill their cursors
    KillAllSessionsByPatternSet patterns;
    {
        stdx::unique_lock<stdx::mutex> lk(_cacheMutex);
        for (auto& lsid : deadSessions) {
            _cache.erase(lsid);
            patterns.emplace(makeKillAllSessionsByPattern(opCtx, lsid));
        }
    }

    // kill cursors owned by deadSessions

    SessionKiller::Matcher matcher(std::move(patterns));
    _service->killCursorsWithMatchingSessions(opCtx, std::move(matcher)).ignore();

    // refresh all reciently active sessions
    auto refreshRes = _sessionsColl->refreshSessions(opCtx, std::move(activeSessions), time);

    if (!refreshRes.isOK()) {
        // TODO SERVER-29709: handle network errors here.
        return Status::OK();
    }

    {
        stdx::unique_lock<stdx::mutex> lk(_cacheMutex);
        lastRefreshTime = time;
    }

    return Status::OK();
}

void LogicalSessionCacheImpl::clear() {
    // TODO: What should this do?  Wasn't implemented before
    MONGO_UNREACHABLE;
}

void LogicalSessionCacheImpl::endSessions(const LogicalSessionIdSet& sessions) {
    stdx::unique_lock<stdx::mutex> lk(_cacheMutex);
    _endingSessions.reserve(_endingSessions.size() + sessions.size());
    for (auto& lsid : sessions) {
        _endingSessions.emplace(lsid);
    }
}

bool LogicalSessionCacheImpl::_isStale(const LogicalSessionRecord& record, Date_t now) const {
    return record.getLastUse() + _sessionTimeout < now;
}

bool LogicalSessionCacheImpl::_isRecientlyActive(const LogicalSessionRecord& record) const {
    return record.getLastUse() >= lastRefreshTime;
}

boost::optional<LogicalSessionRecord> LogicalSessionCacheImpl::_addToCache(
    LogicalSessionRecord record) {
    stdx::unique_lock<stdx::mutex> lk(_cacheMutex);
    return _cache.add(record.getId(), std::move(record));
}

std::vector<LogicalSessionId> LogicalSessionCacheImpl::listIds() const {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    std::vector<LogicalSessionId> ret;
    ret.reserve(_cache.size());
    for (const auto& id : _cache) {
        ret.push_back(id.first);
    }
    return ret;
}

std::vector<LogicalSessionId> LogicalSessionCacheImpl::listIds(
    const std::vector<SHA256Block>& userDigests) const {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    std::vector<LogicalSessionId> ret;
    for (const auto& it : _cache) {
        if (std::find(userDigests.cbegin(), userDigests.cend(), it.first.getUid()) !=
            userDigests.cend()) {
            ret.push_back(it.first);
        }
    }
    return ret;
}

boost::optional<LogicalSessionRecord> LogicalSessionCacheImpl::peekCached(
    const LogicalSessionId& id) const {
    stdx::lock_guard<stdx::mutex> lk(_cacheMutex);
    const auto it = _cache.cfind(id);
    if (it == _cache.cend()) {
        return boost::none;
    }
    return it->second;
}
}  // namespace mongo
