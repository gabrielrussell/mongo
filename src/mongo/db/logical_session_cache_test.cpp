/**
 *    Copyright (C) 2017 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include <sstream>

#include "mongo/platform/basic.h"

#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/authorization_session_for_test.h"
#include "mongo/db/auth/authz_manager_external_state_mock.h"
#include "mongo/db/auth/authz_session_external_state_mock.h"

#include "mongo/bson/oid.h"
#include "mongo/db/auth/user_name.h"
#include "mongo/db/logical_session_cache.h"
#include "mongo/db/logical_session_cache_impl.h"
#include "mongo/db/logical_session_id.h"
#include "mongo/db/logical_session_id_helpers.h"
#include "mongo/db/operation_context_noop.h"
#include "mongo/db/service_context_noop.h"
#include "mongo/db/service_liason_mock.h"
#include "mongo/db/sessions_collection_mock.h"
#include "mongo/stdx/future.h"
#include "mongo/stdx/memory.h"
#include "mongo/unittest/unittest.h"

namespace mongo {
namespace {

const Milliseconds kSessionTimeout = duration_cast<Milliseconds>(kLogicalSessionDefaultTimeout);
const Milliseconds kForceRefresh =
    duration_cast<Milliseconds>(LogicalSessionCacheImpl::kLogicalSessionDefaultRefresh);

using SessionList = std::list<LogicalSessionId>;

/**
 * Test fixture that sets up a session cache attached to a mock service liason
 * and mock sessions collection implementation.
 */
class LogicalSessionCacheTest : public unittest::Test {
public:
    LogicalSessionCacheTest()
        : _service(std::make_shared<MockServiceLiasonImpl>()),
          _sessions(std::make_shared<MockSessionsCollectionImpl>()) {}

    void setUp() override {
        auto localManagerState = stdx::make_unique<AuthzManagerExternalStateMock>();
        auto managerState = localManagerState.get();
        managerState->setAuthzVersion(AuthorizationManager::schemaVersion26Final);
        auto uniqueAuthzManager =
            stdx::make_unique<AuthorizationManager>(std::move(localManagerState));
        AuthorizationManager::set(&serviceContext, std::move(uniqueAuthzManager));

        auto client = serviceContext.makeClient("testClient");
        _opCtx = client->makeOperationContext();
        _client = client.get();
        Client::setCurrent(std::move(client));

        auto mockService = stdx::make_unique<MockServiceLiason>(_service);
        auto mockSessions = stdx::make_unique<MockSessionsCollection>(_sessions);
        _cache = stdx::make_unique<LogicalSessionCacheImpl>(std::move(mockService),
                                                            std::move(mockSessions));
    }

    void tearDown() override {
        if (_opCtx) {
            _opCtx.reset();
        }

        _service->join();
        auto client = Client::releaseCurrent();
    }

    void waitUntilRefreshScheduled() {
        while (service()->jobs() < 2) {
            sleepmillis(10);
        }
    }

    std::unique_ptr<LogicalSessionCache>& cache() {
        return _cache;
    }

    std::shared_ptr<MockServiceLiasonImpl> service() {
        return _service;
    }

    std::shared_ptr<MockSessionsCollectionImpl> sessions() {
        return _sessions;
    }

    void setOpCtx() {
        _opCtx = client()->makeOperationContext();
    }

    void clearOpCtx() {
        _opCtx.reset();
    }

    OperationContext* opCtx() {
        return _opCtx.get();
    }

    Client* client() {
        return _client;
    }

private:
    ServiceContextNoop serviceContext;
    ServiceContext::UniqueOperationContext _opCtx;

    std::shared_ptr<MockServiceLiasonImpl> _service;
    std::shared_ptr<MockSessionsCollectionImpl> _sessions;

    std::unique_ptr<LogicalSessionCache> _cache;

    Client* _client;
};

// Test that the getFromCache method does not make calls to the sessions collection
TEST_F(LogicalSessionCacheTest, TestCacheHitsOnly) {
    auto lsid = makeLogicalSessionIdForTest();

    // When the record is not present (and not in the sessions collection), returns an error
    auto res = cache()->promote(lsid);
    ASSERT(!res.isOK());

    // When the record is not present (but is in the sessions collection), returns an error
    sessions()->add(makeLogicalSessionRecord(lsid, service()->now()));
    res = cache()->promote(lsid);
    ASSERT(!res.isOK());
}

// Test that promoting from the cache updates the lastUse date of records
TEST_F(LogicalSessionCacheTest, PromoteUpdatesLastUse) {
    auto lsid = makeLogicalSessionIdForTest();

    auto start = service()->now();

    // Insert the record into the sessions collection with 'start'
    ASSERT(cache()->startSession(opCtx(), makeLogicalSessionRecord(lsid, start)).isOK());

    // Fast forward time and promote
    service()->fastForward(Milliseconds(500));
    ASSERT(start != service()->now());
    auto res = cache()->promote(lsid);
    ASSERT(res.isOK());

    // Now that we promoted, lifetime of session should be extended
    service()->fastForward(kSessionTimeout - Milliseconds(500));
    res = cache()->promote(lsid);
    ASSERT(res.isOK());

    // We promoted again, so lifetime extended again
    service()->fastForward(kSessionTimeout - Milliseconds(10));
    res = cache()->promote(lsid);
    ASSERT(res.isOK());

    // Fast forward and promote
    service()->fastForward(kSessionTimeout - Milliseconds(10));
    res = cache()->promote(lsid);
    ASSERT(res.isOK());

    // Lifetime extended again
    service()->fastForward(Milliseconds(11));
    res = cache()->promote(lsid);
    ASSERT(res.isOK());

    // Let record expire, we should still be able to get it, since cache didn't get cleared
    service()->fastForward(kSessionTimeout + Milliseconds(1));
    res = cache()->promote(lsid);
    ASSERT(res.isOK());
}

// Test the startSession method
TEST_F(LogicalSessionCacheTest, StartSession) {
    auto record = makeLogicalSessionRecord(makeLogicalSessionIdForTest(), service()->now());
    auto lsid = record.getId();

    // Test starting a new session
    auto res = cache()->startSession(opCtx(), record);
    ASSERT(res.isOK());

    // Record will not be in the collection yet; refresh must happen first.
    ASSERT(!sessions()->has(lsid));

    // Do refresh, cached records should get flushed to collection.
    clearOpCtx();
    ASSERT(cache()->refreshNow(client()).isOK());
    ASSERT(sessions()->has(lsid));

    // Try to start the same session again, should succeed.
    res = cache()->startSession(opCtx(), record);
    ASSERT(res.isOK());

    // Try to start a session that is already in the sessions collection but
    // is not in our local cache, should succeed.
    auto record2 = makeLogicalSessionRecord(makeLogicalSessionIdForTest(), service()->now());
    sessions()->add(record2);
    res = cache()->startSession(opCtx(), record2);
    ASSERT(res.isOK());

    // Try to start a session that has expired from our cache, and is no
    // longer in the sessions collection, should succeed
    service()->fastForward(Milliseconds(kSessionTimeout.count() + 5));
    sessions()->remove(lsid);
    ASSERT(!sessions()->has(lsid));
    res = cache()->startSession(opCtx(), record);
    ASSERT(res.isOK());
}

// Test that session cache properly expires lsids after 30 minutes of no use
TEST_F(LogicalSessionCacheTest, BasicSessionExpiration) {
    // Insert a lsid
    auto record = makeLogicalSessionRecordForTest();
    cache()->startSession(opCtx(), record).transitional_ignore();
    auto res = cache()->promote(record.getId());
    ASSERT(res.isOK());

    // Force it to expire
    service()->fastForward(Milliseconds(kSessionTimeout.count() + 5));

    // Check that it is no longer in the cache
    ASSERT(cache()->refreshNow(client()).isOK());
    res = cache()->promote(record.getId());
    // TODO SERVER-29709
    // ASSERT(!res.isOK());
}

// Test large sets of cache-only session lsids
TEST_F(LogicalSessionCacheTest, ManySignedLsidsInCacheRefresh) {
    int count = LogicalSessionCacheImpl::kLogicalSessionCacheDefaultCapacity;
    for (int i = 0; i < count; i++) {
        auto record = makeLogicalSessionRecordForTest();
        cache()->startSession(opCtx(), record).transitional_ignore();
    }

    // Check that all signedLsids refresh
    sessions()->setRefreshHook([&count](const LogicalSessionRecordSet& sessions) {
        ASSERT_EQ(sessions.size(), size_t(count));
        return Status::OK();
    });

    // Force a refresh
    clearOpCtx();
    service()->fastForward(kForceRefresh);
    ASSERT(cache()->refreshNow(client()).isOK());
}

//
TEST_F(LogicalSessionCacheTest, RefreshMatrixSessionState) {
    std::string stateNames[4][2] = {
        {"active", "inactive"},
        {"running", "not running"},
        {"expired", "unexpired"},
        {"ended", "not ended"},
    };
    struct {
        // results that we test for after the _refresh
        bool inCollection;
        bool killed;
    } testCases[16] = {
        // 0, active, running, expired, ended,
        {false, true},
        // 1, inactive, running, expired, ended,
        {false, true},
        // 2, active, not running, expired, ended,
        {false, true},
        // 3, inactive, not running, expired, ended,
        {false, true},
        // 4, active, running, unexpired, ended,
        {false, true},
        // 5, inactive, running, unexpired, ended,
        {false, true},
        // 6, active, not running, unexpired, ended,
        {false, true},
        // 7, inactive, not running, unexpired, ended,
        {false, true},
        // 8, active, running, expired, not ended,
        {true, false},
        // 9, inactive, running, expired, not ended,
        {false, true},
        // 10, active, not running, expired, not ended,
        {true, false},
        // 11, inactive, not running, expired, not ended,
        {false, false},
        // 12, active, running, unexpired, not ended,
        {true, false},
        // 13, inactive, running, unexpired, not ended,
        {true, false},
        // 14, active, not running, unexpired, not ended,
        {true, false},
        // 15, inactive, not running, unexpired, not ended,
        {true, false},
    };

    std::vector<LogicalSessionId> ids;
    for (int i = 0; i < 16; i++) {

        bool active = !(i & 1);
        bool running = !(i & 2);
        bool expired = !(i & 4);
        bool ended = !(i & 8);

        auto lsid = makeLogicalSessionIdForTest();
        ids.push_back(lsid);
        auto lsRecord = makeLogicalSessionRecord(lsid, service()->now() + Milliseconds(500));

        if (running) {
            service()->add(lsid);
        }
        if (active) {
            cache()->startSession(opCtx(), lsRecord);
        }
        if (!expired) {
            sessions()->add(lsRecord);
        }
        if (ended) {
            LogicalSessionIdSet lsidSet;
            lsidSet.emplace(lsid);
            cache()->endSessions(lsidSet);
        }
    }

    // Force a refresh
    clearOpCtx();
    service()->fastForward(kForceRefresh);
    ASSERT(cache()->refreshNow(client()).isOK());

    for (int i = 0; i < 16; i++) {
        bool failed = false;
        std::stringstream failCauseText;
        if (sessions()->has(ids[i]) != testCases[i].inCollection) {
            failed = true;
            if (testCases[i].inCollection) {
                failCauseText << "session wasn't in collection";
            } else {
                failCauseText << "session was in collection";
            }
        }
        if ((service()->matchKilled(ids[i]) != nullptr) != testCases[i].killed) {
            failed = true;
            if (failCauseText.str() != "") {
                failCauseText << " and ";
            }
            if (testCases[i].killed) {
                failCauseText << "session wasn't killed";
            } else {
                failCauseText << "session was killed";
            }
        }
        if (failed) {
            std::stringstream failText;
            failText << "case " << i << " : ";
            for (int j = 0; j < 4; j++) {
                failText << stateNames[j][i >> j & 1] << " ";
            }
            failText << " session case failed: " << failCauseText.str();
            FAIL(failText.str());
        }
    }
}


}  // namespace
}  // namespace mongo
