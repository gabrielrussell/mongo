/*
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

#include "mongo/base/init.h"
#include "mongo/db/auth/action_set.h"
#include "mongo/db/auth/action_type.h"
#include "mongo/db/auth/authorization_manager.h"
#include "mongo/db/auth/authorization_session.h"
#include "mongo/db/auth/privilege.h"
#include "mongo/db/client.h"
#include "mongo/db/commands.h"
#include "mongo/db/jsobj.h"
#include "mongo/db/logical_session_id.h"
#include "mongo/db/logical_session_record.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/stats/top.h"
#include "mongo/platform/basic.h"

namespace {

using namespace mongo;

class StartSessionCommand : public Command {
public:
    StartSessionCommand() : Command("startSession") {}

    virtual bool slaveOk() const {
        return true;
    }
    virtual bool adminOnly() const {
        return false;
    }
    virtual bool supportsWriteConcern(const BSONObj& cmd) const override {
        return false;
    }
    virtual void help(std::stringstream& help) const {
        help << "start a logical session";
    }
    virtual void addRequiredPrivileges(const std::string& dbname,
                                       const BSONObj& cmdObj,
                                       std::vector<Privilege>* out) {
        ActionSet actions;
        actions.addAction(ActionType::startSession);
        out->push_back(Privilege(ResourcePattern::forClusterResource(), actions));
    }
    virtual bool run(OperationContext* opCtx,
                     const std::string& db,
                     const BSONObj& cmdObj,
                     std::string& errmsg,
                     BSONObjBuilder& result) {
        {
            try {  // XXX should I leave this try catch std::terminate() stuff?
                // XXX remove the Username once it leaves the makeAuthoritativeRecord api
                UserName userName("", "");
                boost::optional<OID> uid;
                auto client = opCtx->getClient();

                ServiceContext* serviceContext = opCtx->getClient()->getServiceContext();
                if (AuthorizationManager::get(serviceContext)->isAuthEnabled()) {

                    auto userNameItr =
                        AuthorizationSession::get(client)->getAuthenticatedUserNames();
                    if (userNameItr.more()) {
                        userName = userNameItr.next();
                        if (userNameItr.more()) {
                            return appendCommandStatus(
                                result,
                                Status(ErrorCodes::AuthenticationFailed,
                                       "must only be authenticated as one user "
                                       "to create a logical session"));
                        }
                    } else {
                        // This shoud really never be reached.
                        // It means that while auth is on, we're not authenticated as any users.
                        // XXX Should we actually assert here or fail more spectacularly
                        return appendCommandStatus(result,
                                                   Status(ErrorCodes::AuthenticationFailed,
                                                          "must only be authenticated as one user "
                                                          "to create a logical session"));
                    }
                }
                auto serviceCtx = client->getServiceContext();

                auto lsRecord = LogicalSessionRecord::makeAuthoritativeRecord(
                    LogicalSessionId::gen(),
                    std::move(userName),
                    uid,
                    serviceCtx->getFastClockSource()->now());
                return appendCommandStatus(
                    result, serviceCtx->getLogicalSessionCache()->startSession(lsRecord));
            } catch (...) {
                std::terminate();
            }
        }
    }
};

//
// Command instance.
// Registers command with the command system and make command
// available to the client.
//

MONGO_INITIALIZER(RegisterStartSessionCommand)(InitializerContext* context) {
    new StartSessionCommand();

    return Status::OK();
}
}  // namespace
