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
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#include "mongo/client/embedded/libmongodbcapi.h"

#include <exception>
#include <thread>
#include <unordered_map>
#include <vector>

#include "mongo/client/embedded/embedded.h"
#include "mongo/db/client.h"
#include "mongo/db/dbmain.h"
#include "mongo/db/service_context.h"
#include "mongo/stdx/memory.h"
#include "mongo/stdx/unordered_map.h"
#include "mongo/transport/service_entry_point.h"
#include "mongo/transport/transport_layer_mock.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/net/message.h"
#include "mongo/util/scopeguard.h"
#include "mongo/util/shared_buffer.h"

struct libmongodbcapi_status {
    libmongodbcapi_error error = LIBMONGODB_CAPI_SUCCESS;
    int exception_code = mongo::ErrorCodes::OK;
    std::string what = "";
};

struct libmongodbcapi_lib {
    libmongodbcapi_lib() = default;
    libmongodbcapi_status status;
};

struct libmongodbcapi_db {
    libmongodbcapi_db() = default;

    libmongodbcapi_db(const libmongodbcapi_db&) = delete;
    libmongodbcapi_db& operator=(const libmongodbcapi_db&) = delete;

    mongo::ServiceContext* serviceContext = nullptr;
    mongo::stdx::unordered_map<libmongodbcapi_client*, std::unique_ptr<libmongodbcapi_client>>
        open_clients;
    std::unique_ptr<mongo::transport::TransportLayerMock> transportLayer;

    std::vector<std::unique_ptr<char[]>> argvStorage;
    std::vector<char*> argvPointers;
    std::vector<std::unique_ptr<char[]>> envpStorage;
    std::vector<char*> envpPointers;

    libmongodbcapi_status status;
    libmongodbcapi_lib* parent_lib = nullptr;
};

struct libmongodbcapi_client {
    libmongodbcapi_client(libmongodbcapi_db* db) : parent_db(db) {}

    libmongodbcapi_client(const libmongodbcapi_client&) = delete;
    libmongodbcapi_client& operator=(const libmongodbcapi_client&) = delete;

    void* client_handle = nullptr;
    std::vector<unsigned char> output;
    mongo::ServiceContext::UniqueClient client;
    mongo::DbResponse response;

    libmongodbcapi_db* parent_db = nullptr;
    libmongodbcapi_status status;
};

namespace mongo {
namespace {

bool libraryInitialized_ = false;
libmongodbcapi_db* global_db = nullptr;

libmongodbcapi_status process_status;

libmongodbcapi_lib* capi_lib_init(const char* yaml_config) {

    libmongodbcapi_lib* lib = new libmongodbcapi_lib;

    if (mongo::libraryInitialized_) {
        process_status.error = LIBMONGODB_CAPI_ERROR_LIBRARY_ALREADY_INITIALIZED;
        lib->status.what = "";
        lib->status.exception_code = mongo::ErrorCodes::InternalError;
        return nullptr;
    }

    mongo::libraryInitialized_ = true;
    return lib;
}

libmongodbcapi_error capi_lib_fini(libmongodbcapi_lib* lib) {
    if (!mongo::libraryInitialized_)
        return LIBMONGODB_CAPI_ERROR_LIBRARY_NOT_INITIALIZED;

    if (mongo::global_db)
        return LIBMONGODB_CAPI_ERROR_DB_OPEN;

    delete lib;
    return LIBMONGODB_CAPI_SUCCESS;
}


libmongodbcapi_db* db_new(libmongodbcapi_lib* lib,
                          int argc,
                          const char** argv,
                          const char** envp) noexcept try {
    if (!libraryInitialized_) {
        lib->status.error = LIBMONGODB_CAPI_ERROR_LIBRARY_ALREADY_INITIALIZED;
        lib->status.what = "";
        lib->status.exception_code = mongo::ErrorCodes::InternalError;
        return nullptr;
    }
    if (global_db) {
        lib->status.error = LIBMONGODB_CAPI_ERROR_DB_OPEN;
        lib->status.what = "";
        lib->status.exception_code = mongo::ErrorCodes::InternalError;
        return nullptr;
    }

    global_db = new libmongodbcapi_db;
    global_db->parent_lib = lib;

    // iterate over argv and copy them to argvStorage
    for (int i = 0; i < argc; i++) {
        // allocate space for the null terminator
        auto s = mongo::stdx::make_unique<char[]>(std::strlen(argv[i]) + 1);
        // copy the string + null terminator
        std::strncpy(s.get(), argv[i], std::strlen(argv[i]) + 1);
        global_db->argvPointers.push_back(s.get());
        global_db->argvStorage.push_back(std::move(s));
    }
    global_db->argvPointers.push_back(nullptr);

    // iterate over envp and copy them to envpStorage
    while (envp != nullptr && *envp != nullptr) {
        auto s = mongo::stdx::make_unique<char[]>(std::strlen(*envp) + 1);
        std::strncpy(s.get(), *envp, std::strlen(*envp) + 1);
        global_db->envpPointers.push_back(s.get());
        global_db->envpStorage.push_back(std::move(s));
        envp++;
    }
    global_db->envpPointers.push_back(nullptr);

    global_db->serviceContext =
        embedded::initialize(argc, global_db->argvPointers.data(), global_db->envpPointers.data());
    if (!global_db->serviceContext) {
        delete global_db;
        global_db = nullptr;
        lib->status.error = LIBMONGODB_CAPI_ERROR_DB_INITIALIZATION_FAILED;
        lib->status.what = "";
        lib->status.exception_code = mongo::ErrorCodes::InternalError;
        return nullptr;
    }

    // creating mock transport layer to be able to create sessions
    global_db->transportLayer = stdx::make_unique<transport::TransportLayerMock>();

    lib->status.error = LIBMONGODB_CAPI_SUCCESS;
    return global_db;
} catch (const DBException& ex) {
    lib->status.error = LIBMONGODB_CAPI_ERROR_EXCEPTION;
    lib->status.what = ex.what();
    lib->status.exception_code = ex.code();
    return nullptr;
} catch (const std::exception& ex) {
    lib->status.error = LIBMONGODB_CAPI_ERROR_EXCEPTION;
    lib->status.what = ex.what();
    lib->status.exception_code = mongo::ErrorCodes::InternalError;
    return nullptr;
}

libmongodbcapi_error db_destroy(libmongodbcapi_db* db) noexcept {
    libmongodbcapi_lib* lib = db->parent_lib;
    try {
        if (!db->open_clients.empty()) {
            lib->status.error = LIBMONGODB_CAPI_ERROR_UNKNOWN;
            lib->status.what = "";
            lib->status.exception_code = mongo::ErrorCodes::InternalError;
            return LIBMONGODB_CAPI_ERROR_UNKNOWN;
        }

        embedded::shutdown(global_db->serviceContext);

        delete db;
        invariant(!db || db == global_db);
        if (db) {
            global_db = nullptr;
        }
        lib->status.error = LIBMONGODB_CAPI_SUCCESS;
        return LIBMONGODB_CAPI_SUCCESS;
    } catch (const DBException& ex) {
        lib->status.error = LIBMONGODB_CAPI_ERROR_EXCEPTION;
        lib->status.what = ex.what();
        lib->status.exception_code = ex.code();
        return LIBMONGODB_CAPI_ERROR_EXCEPTION;
    } catch (const std::exception& ex) {
        lib->status.error = LIBMONGODB_CAPI_ERROR_EXCEPTION;
        lib->status.what = ex.what();
        lib->status.exception_code = mongo::ErrorCodes::InternalError;
        return LIBMONGODB_CAPI_ERROR_EXCEPTION;
    }
}


libmongodbcapi_client* client_new(libmongodbcapi_db* db) noexcept try {
    auto new_client = stdx::make_unique<libmongodbcapi_client>(db);
    libmongodbcapi_client* rv = new_client.get();
    db->open_clients.insert(std::make_pair(rv, std::move(new_client)));

    auto session = global_db->transportLayer->createSession();
    rv->client = global_db->serviceContext->makeClient("embedded", std::move(session));

    return rv;
} catch (const DBException& ex) {
    db->status.error = LIBMONGODB_CAPI_ERROR_EXCEPTION;
    db->status.what = ex.what();
    db->status.exception_code = ex.code();
    return nullptr;
} catch (const std::exception& ex) {
    db->status.error = LIBMONGODB_CAPI_ERROR_EXCEPTION;
    db->status.what = ex.what();
    db->status.exception_code = mongo::ErrorCodes::InternalError;
    return nullptr;
}

libmongodbcapi_error client_destroy(libmongodbcapi_client* client) noexcept {
    if (!client) {
        return LIBMONGODB_CAPI_SUCCESS;
    }
    client->parent_db->open_clients.erase(client);
    return LIBMONGODB_CAPI_SUCCESS;
}

libmongodbcapi_error client_wire_protocol_rpc(libmongodbcapi_client* client,
                                              const void* input,
                                              size_t input_size,
                                              void** output,
                                              size_t* output_size) noexcept try {
    mongo::Client::setCurrent(std::move(client->client));
    const auto guard = mongo::MakeGuard([&] { client->client = mongo::Client::releaseCurrent(); });

    auto opCtx = cc().makeOperationContext();
    auto sep = client->parent_db->serviceContext->getServiceEntryPoint();

    auto sb = SharedBuffer::allocate(input_size);
    memcpy(sb.get(), input, input_size);

    Message msg(std::move(sb));

    client->response = sep->handleRequest(opCtx.get(), msg);
    *output_size = client->response.response.size();
    *output = (void*)client->response.response.buf();

    return LIBMONGODB_CAPI_SUCCESS;
} catch (const DBException& ex) {
    client->status.error = LIBMONGODB_CAPI_ERROR_EXCEPTION;
    client->status.what = ex.what();
    client->status.exception_code = ex.code();
    return LIBMONGODB_CAPI_ERROR_EXCEPTION;
} catch (const std::exception& ex) {
    client->status.error = LIBMONGODB_CAPI_ERROR_EXCEPTION;
    client->status.what = ex.what();
    client->status.exception_code = mongo::ErrorCodes::InternalError;
    return LIBMONGODB_CAPI_ERROR_EXCEPTION;
}


libmongodbcapi_error capi_status_get_error(libmongodbcapi_status* status) noexcept {
    return status->error;
}

const char* capi_status_get_what(libmongodbcapi_status* status) noexcept {
    return status->what.c_str();
}

int capi_status_get_code(libmongodbcapi_status* status) noexcept {
    return status->exception_code;
}

libmongodbcapi_status* capi_process_get_status() noexcept {
    return &process_status;
}

libmongodbcapi_status* capi_lib_get_status(libmongodbcapi_lib* lib) noexcept {
    return &(lib->status);
}

libmongodbcapi_status* capi_db_get_status(libmongodbcapi_db* db) noexcept {
    return &(db->status);
}

libmongodbcapi_status* capi_client_get_status(libmongodbcapi_client* client) noexcept {
    return &(client->status);
}

}  // namespace
}  // namespace mongo

extern "C" {

libmongodbcapi_lib* libmongodbcapi_init(const char* yaml_config) {
    return mongo::capi_lib_init(yaml_config);
}

int libmongodbcapi_fini(libmongodbcapi_lib* lib) {
    return mongo::capi_lib_fini(lib);
}

libmongodbcapi_db* libmongodbcapi_db_new(libmongodbcapi_lib* lib,
                                         int argc,
                                         const char** argv,
                                         const char** envp) {
    return mongo::db_new(lib, argc, argv, envp);
}

int libmongodbcapi_db_destroy(libmongodbcapi_db* db) {
    return mongo::db_destroy(db);
}

libmongodbcapi_client* libmongodbcapi_db_client_new(libmongodbcapi_db* db) {
    return mongo::client_new(db);
}

libmongodbcapi_error libmongodbcapi_db_client_destroy(libmongodbcapi_client* client) {
    return mongo::client_destroy(client);
}

int libmongodbcapi_db_client_wire_protocol_rpc(libmongodbcapi_client* client,
                                               const void* input,
                                               size_t input_size,
                                               void** output,
                                               size_t* output_size) {
    return mongo::client_wire_protocol_rpc(client, input, input_size, output, output_size);
}

libmongodbcapi_error libmongodbcapi_status_get_error(libmongodbcapi_status* status) {
    return mongo::capi_status_get_error(status);
}

const char* libmongodbcapi_status_get_what(libmongodbcapi_status* status) {
    return mongo::capi_status_get_what(status);
}

int libmongodbcapi_status_get_code(libmongodbcapi_status* status) {
    return mongo::capi_status_get_code(status);
}

libmongodbcapi_status* libmongodbcapi_process_get_status() {
    return mongo::capi_process_get_status();
}

libmongodbcapi_status* libmongodbcapi_lib_get_status(libmongodbcapi_lib* lib) {
    return mongo::capi_lib_get_status(lib);
}

libmongodbcapi_status* libmongodbcapi_db_get_status(libmongodbcapi_db* db) {
    return mongo::capi_db_get_status(db);
}

libmongodbcapi_status* libmongodbcapi_client_get_status(libmongodbcapi_client* client) {
    return mongo::capi_client_get_status(client);
}
}
