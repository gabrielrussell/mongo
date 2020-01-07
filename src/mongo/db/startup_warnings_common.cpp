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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kControl

#include "mongo/platform/basic.h"

#include "mongo/db/startup_warnings_common.h"

#include <boost/filesystem/operations.hpp>
#include <fstream>

#include "mongo/client/authenticate.h"
#include "mongo/config.h"
#include "mongo/db/server_options.h"
#include "mongo/util/log.h"
#include "mongo/util/net/ssl_options.h"
#include "mongo/util/processinfo.h"
#include "mongo/util/version.h"

namespace mongo {

//
// system warnings
//
void logCommonStartupWarnings(const ServerGlobalParams& serverParams) {
    // each message adds a leading and a trailing newline

    bool warned = false;
    {
        auto&& vii = VersionInfoInterface::instance();
        if ((vii.minorVersion() % 2) != 0) {
            LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "");
            LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** NOTE: This is a development version ({}) of MongoDB.", "vii_version"_attr = vii.version());
            LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**       Not recommended for production.");
            warned = true;
        }
    }

    if (serverParams.authState == ServerGlobalParams::AuthState::kUndefined) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** WARNING: Access control is not enabled for the database.");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**          Read and write access to data and configuration is "
                 "unrestricted.");
        warned = true;
    }

    const bool is32bit = sizeof(int*) == 4;
    if (is32bit) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** WARNING: This 32-bit MongoDB binary is deprecated");
        warned = true;
    }

#ifdef MONGO_CONFIG_SSL
    if (sslGlobalParams.sslAllowInvalidCertificates) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** WARNING: While invalid X509 certificates may be used to");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**          connect to this server, they will not be considered");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**          permissible for authentication.");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "");
    }

    if (sslGlobalParams.sslAllowInvalidHostnames) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** WARNING: This server will not perform X.509 hostname validation");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** This may allow your server to make or accept connections to");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** untrusted parties");
    }
#endif

    /*
     * We did not add the message to startupWarningsLog as the user can not
     * specify a sslCAFile parameter from the shell
     */
    if (sslGlobalParams.sslMode.load() != SSLParams::SSLMode_disabled &&
#ifdef MONGO_CONFIG_SSL_CERTIFICATE_SELECTORS
        sslGlobalParams.sslCertificateSelector.empty() &&
#endif
        sslGlobalParams.sslCAFile.empty()) {
        LOGV2("");
        LOGV2("** WARNING: No client certificate validation can be performed since"
                 " no CA file has been provided");
#ifdef MONGO_CONFIG_SSL_CERTIFICATE_SELECTORS
        LOGV2("**          and no sslCertificateSelector has been specified.");
#endif
        LOGV2("**          Please specify an sslCAFile parameter.");
    }

#if defined(_WIN32) && !defined(_WIN64)
    // Warn user that they are running a 32-bit app on 64-bit Windows
    BOOL wow64Process;
    BOOL retWow64 = IsWow64Process(GetCurrentProcess(), &wow64Process);
    if (retWow64 && wow64Process) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** NOTE: This is a 32-bit MongoDB binary running on a 64-bit operating");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**      system. Switch to a 64-bit build of MongoDB to");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**      support larger databases.");
        warned = true;
    }
#endif

#if !defined(_WIN32)
    if (getuid() == 0) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** WARNING: You are running this process as the root user, which is not recommended.");
        warned = true;
    }
#endif

    if (serverParams.bind_ips.empty()) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** WARNING: This server is bound to localhost.");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**          Remote systems will be unable to connect to this server. ");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**          Start the server with --bind_ip <address> to specify which IP ");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**          addresses it should serve responses from, or with --bind_ip_all to");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**          bind to all interfaces. If this behavior is desired, start the");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "**          server with --bind_ip 127.0.0.1 to disable this warning.");
        warned = true;
    }

    if (auth::hasMultipleInternalAuthKeys()) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "** WARNING: Multiple keys specified in security key file. If cluster key file");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "            rollover is not in progress, only one key should be specified in");
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "            the key file");
        warned = true;
    }

    if (warned) {
        LOGV2_OPTIONS({logv2::LogTag::kStartupWarnings}, "");
    }
}
}  // namespace mongo
