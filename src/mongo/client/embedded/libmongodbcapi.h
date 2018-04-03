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
#ifndef LIBMONGODBCAPI_H
#define LIBMONGODBCAPI_H

#include <stddef.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmongodbcapi_status libmongodbcapi_status;
typedef struct libmongodbcapi_lib libmongodbcapi_lib;
typedef struct libmongodbcapi_db libmongodbcapi_db;
typedef struct libmongodbcapi_client libmongodbcapi_client;

typedef enum {
    LIBMONGODB_CAPI_ERROR_UNKNOWN = -1,
    LIBMONGODB_CAPI_SUCCESS = 0,

    LIBMONGODB_CAPI_ERROR_DB_INITIALIZATION_FAILED,
    LIBMONGODB_CAPI_ERROR_LIBRARY_ALREADY_INITIALIZED,
    LIBMONGODB_CAPI_ERROR_LIBRARY_NOT_INITIALIZED,
    LIBMONGODB_CAPI_ERROR_DB_OPEN,
    LIBMONGODB_CAPI_ERROR_EXCEPTION,
} libmongodbcapi_error;


/**
* Initializes the mongodbcapi library, required before any other call. Cannot be called again
* without libmongodbcapi_fini() being called first.
*
* @param config null-terminated YAML formatted MongoDB configuration. See documentation for valid
* options.
*
* @note This function is not thread safe.
*
* @return Returns LIBMONGODB_CAPI_SUCCESS on success.
* @return Returns LIBMONGODB_CAPI_ERROR_LIBRARY_ALREADY_INITIALIZED if libmongodbcapi_init() has
* already been called without an intervening call to libmongodbcapi_fini().
*/
libmongodbcapi_lib* libmongodbcapi_init(const char* yaml_config);

/**
* Tears down the state of the library, all databases must be closed before calling this.
*
* @note This function is not thread safe.
*
* @return Returns LIBMONGODB_CAPI_SUCCESS on success.
* @return Returns LIBMONGODB_CAPI_ERROR_LIBRARY_NOT_INITIALIZED if libmongodbcapi_init() has not
* been called previously.
* @return Returns LIBMONGODB_CAPI_ERROR_DB_OPEN if there are open databases that haven't been closed
* with libmongodbcapi_db_destroy().
* @return Returns LIBMONGODB_CAPI_ERROR_EXCEPTION for errors that resulted in an exception. The
* strigified version of the exception can be retrieved with
* libmongodbcapi_get_last_error_exception_string()
* @return Returns LIBMONGODB_CAPI_ERROR_UNKNOWN for any other unspecified errors.
*/
int libmongodbcapi_fini(libmongodbcapi_lib*);

/**
* Starts the database and returns a handle with the service context.
*
* @param argc
*      The number of arguments in argv
* @param argv
*      The arguments that will be passed to mongod at startup to initialize state
* @param envp
*      Environment variables that will be passed to mongod at startup to initilize state
*
* @return A pointer to a db handle or null on error
*/
libmongodbcapi_db* libmongodbcapi_db_new(libmongodbcapi_lib* lib,
                                         int argc,
                                         const char** argv,
                                         const char** envp);

/**
* Shuts down the database
*
* @param
*       A pointer to a db handle to be destroyed
*
* @return A libmongo error code
*/
int libmongodbcapi_db_destroy(libmongodbcapi_db* db);

/**
* Creates a new clienst and retuns it so the caller can do operation
* A client will be destroyed when the owning db is destroyed
*
* @param db
*      The datadase that will own this client and execute its RPC calls
*
* @return A pointer to a client or null on error
*/
libmongodbcapi_client* libmongodbcapi_db_client_new(libmongodbcapi_db* db);

/**
* Destroys a client and removes it from the db/service context
* Cannot be called after the owning db is destroyed
*
* @param client
*       A pointer to the client to be destroyed
*/
libmongodbcapi_error libmongodbcapi_db_client_destroy(libmongodbcapi_client* client);

/**
* Makes an RPC call to the database
*
* @param client
*      The client that will be performing the query on the database
* @param input
*      The query to be sent to and then executed by the database
* @param input_size
*      The size (number of bytes) of the input query
* @param output
*      A pointer to a void * where the database can write the location of the output.
*      The library will manage the memory pointer to by *output.
*      @TODO document lifetime of this buffer
* @param output_size
*      A pointer to a location where this function will write the size (number of bytes)
*      of the output
*
* @return Returns LIBMONGODB_CAPI_SUCCESS on success, or an error code from libmongodbcapi_error on
* failure.
*/
int libmongodbcapi_db_client_wire_protocol_rpc(libmongodbcapi_client* client,
                                               const void* input,
                                               size_t input_size,
                                               void** output,
                                               size_t* output_size);
/**
* @return Returns the libmongodbcapi_error of the libmongodbcapi_status. If the failing function
* returns a libmongodbcapi_status, then this is redundant, but if the failing function returns
* pointer, then this is where you find out the libmongodbcapi_error for the failure.
*/

libmongodbcapi_error libmongodbcapi_status_get_error(libmongodbcapi_status* status);

/**
* @return For failures where the libmongodbcapi_error==LIBMONGODB_CAPI_ERROR_EXCEPTION, this
* returns a string representation of the exception
*/

const char* libmongodbcapi_status_get_what(libmongodbcapi_status* status);

/**
* @return For failures where the libmongodbcapi_error==LIBMONGODB_CAPI_ERROR_EXCEPTION and the
* exception was of type DBException, this returns the numeric code indicating which specific
* DBException was thrown
*/

int libmongodbcapi_status_get_code(libmongodbcapi_status* status);

/**
* @return Returns a pointer to the processes libmongodbcapi_status which will hold details of
* any failures from creating or destroying libmongodbcapi_lib's.
*/

libmongodbcapi_status* libmongodbcapi_process_get_status();

/**
* @return Returns pointer to the mongodbcapi_lib's libmongodbcapi_status which will hold details
* of any failures of creating or destroying libmongodbcapi_db's, or from acting directly on the
* mongodbcapi_lib
* */

libmongodbcapi_status* libmongodbcapi_lib_get_status(libmongodbcapi_lib* lib);

/**
* @return Returns pointer to the mongodbcapi_db's libmongodbcapi_status which will hold details
* of any failures of creating or destroying libmongodbcapi_db's, or from acting directly on the 
* libmongodbcapi_db
* */

libmongodbcapi_status* libmongodbcapi_db_get_status(libmongodbcapi_db* db);

/**
* @return Returns pointer to the mongodbcapi_db's libmongodbcapi_status which will hold details
* of any failures from acting directly on the libmongodbcapi_client
* */

libmongodbcapi_status* libmongodbcapi_client_get_status(libmongodbcapi_client* client);


#ifdef __cplusplus
}
#endif

#endif
