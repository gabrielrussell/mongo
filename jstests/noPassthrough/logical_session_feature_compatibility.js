(function() {
    'use strict';
    var conn;
    var admin;
    var foo;
    var result;
    var request = {startSession: 1};

    conn = MongoRunner.runMongod({nojournal: ""});
    admin = conn.getDB("admin");

    admin.adminCommand({ setFeatureCompatibilityVersion:"3.6" })

    result = admin.runCommand(request);
    assert.commandWorked(
        result,
        "failed test that we can run startSession under featureCompatibilityVersion 3.6");

    admin.adminCommand({ setFeatureCompatibilityVersion:"3.4" })

    result = admin.runCommand(request);
    assert.commandFailedWithCode( result, ErrorCodes.InvalidOptions,
        "failed test that we can't run startSession under featureCompatibilityVersion 3.4");

    admin.createUser({user: 'user0', pwd: 'password', roles: jsTest.basicUserRoles});
    admin.createUser({user: 'admin0', pwd: 'password', roles: jsTest.adminUserRoles});

    MongoRunner.stopMongod(conn);

    conn = MongoRunner.runMongod({auth: "", nojournal: ""});

    admin.adminCommand({ setFeatureCompatibilityVersion: "3.4" })

    result = admin.runCommand(request);
    assert.commandFailedWithCode(
        result,
        ErrorCodes.Unauthorized,
        "failed test that we get an auth failure running startSession without being authed as a user against a server running with --auth and under featureCompatibilityVersion 3.6");

    admin.adminCommand({ setFeatureCompatibilityVersion: "3.6" })

    result = admin.runCommand(request);
    assert.commandFailedWithCode(
        result,
        ErrorCodes.InvalidOptions,
        "failed test that we get an InvalidOptions failure running startSession without being authed as a user against a server running with --auth and under featureCompatibilityVersion 3.4");

    admin.auth("user0", "password");

    //admin.auth("user0", "password");

    MongoRunner.stopMongod(conn);
})();
