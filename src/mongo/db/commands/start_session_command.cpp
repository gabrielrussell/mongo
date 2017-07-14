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

#include "mongo/platform/basic.h"

#include "mongo/base/init.h"
#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/client.h"
#include "mongo/db/commands.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/logical_session_cache.h"
#include "mongo/db/logical_session_id.h"
#include "mongo/db/logical_session_record.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/stats/top.h"

namespace mongo {

class StartSessionCommand final : public BasicCommand {
    MONGO_DISALLOW_COPYING(StartSessionCommand);

public:
    StartSessionCommand() : BasicCommand("startSession") {}

    bool slaveOk() const {
        return true;
    }
    bool adminOnly() const {
        return false;
    }
    bool supportsWriteConcern(const BSONObj& cmd) const {
        return false;
    }
    void help(std::stringstream& help) const {
        help << "start a logical session";
    }
    Status checkAuthForOperation(OperationContext* opCtx,
                                 const std::string& dbname,
                                 const BSONObj& cmdObj) {
        AuthorizationSession* authSession = AuthorizationSession::get(opCtx->getClient());
        if (!authSession->isAuthorizedForActionsOnResource(ResourcePattern::forClusterResource(),
                                                           ActionType::startSession)) {
            return Status(ErrorCodes::Unauthorized, "Unauthorized");
        }
        return Status::OK();
    }

    virtual bool run(OperationContext* opCtx,
                     const std::string& db,
                     const BSONObj& cmdObj,
                     BSONObjBuilder& result) {

        boost::optional<OID> uid;
        auto client = opCtx->getClient();

        ServiceContext* serviceContext = client->getServiceContext();
        if (AuthorizationManager::get(serviceContext)->isAuthEnabled()) {

            UserName userName("", "");

            auto authzSession = AuthorizationSession::get(client);
            auto userNameItr = authzSession->getAuthenticatedUserNames();
            if (userNameItr.more()) {
                userName = userNameItr.next();
                if (userNameItr.more()) {
                    return appendCommandStatus(
                        result,
                        Status(ErrorCodes::Unauthorized,
                               "must only be authenticated as exactly one user "
                               "to create a logical session"));
                }
            } else {
                return appendCommandStatus(result,
                                           Status(ErrorCodes::Unauthorized,
                                                  "must only be authenticated as exactly one user "
                                                  "to create a logical session"));
            }

            User* user = authzSession->lookupUser(userName);
            invariant(user);
            uid = user->getID();
        }

        auto lsCache = LogicalSessionCache::get(serviceContext);

        auto statusWithSlsid = lsCache->signLsid(opCtx, LogicalSessionId::gen(), uid);
        if (!statusWithSlsid.isOK()) {
            return appendCommandStatus(result, statusWithSlsid.getStatus());
        }
        auto slsid = statusWithSlsid.getValue();
        auto lsRecord = LogicalSessionRecord::makeAuthoritativeRecord(
            slsid, serviceContext->getFastClockSource()->now());
        Status startSessionStatus = lsCache->startSession(std::move(slsid));

        appendCommandStatus(result, startSessionStatus);

        if (startSessionStatus.isOK()) {
            result.append("id", lsRecord.toBSON());
            result.append("timeoutMinutes", localLogicalSessionTimeoutMinutes);
            return true;
        }

        return false;
    }
} startSessionCommand;

}  // namespace mongo
